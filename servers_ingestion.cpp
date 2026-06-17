#include "ui/controllers/start_server_controller.h"
#include "ui/controllers/coordinator_http_server.h"
#include "packet_json_helpers.h"

#include <unordered_set>
#include <utility>

namespace Sample::UI::Controllers
{

static bool SetIfDifferent(std::string& dst, const std::string& src)
{
   if (src.empty() || dst == src)
      return false;

   dst = src;
   return true;
}

static bool SetBoolIfDifferent(bool& dst, bool src)
{
   if (dst == src)
      return false;

   dst = src;
   return true;
}

static bool ApplyServerInfoToSC(SCSessionState& sc, const Json& serverInfo)
{
   bool changed = false;
   changed |= SetIfDifferent(sc.connectionInfo, JsonGetString(serverInfo, { "connectionInfo" }));
   changed |= SetIfDifferent(sc.serverProperty, JsonGetString(serverInfo, { "serverProperty" }));
   return changed;
}

static bool ApplyServerInfoToLinkedServer(LiveState& st, const SCSessionState& sc)
{
   if (sc.serverId.empty())
      return false;

   auto it = st.servers.find(sc.serverId);
   if (it == st.servers.end())
      return false;

   bool changed = false;
   changed |= SetIfDifferent(it->second.connectionInfo, sc.connectionInfo);
   changed |= SetIfDifferent(it->second.serverProperty, sc.serverProperty);
   return changed;
}

static bool AttachHydraContextToSC(LiveState& st, SCSessionState& sc,
   const std::string& hydraUserId,
   const std::string& hydraKernelSessionId,
   const std::string& dataCenterId = {})
{
   bool changed = false;

   if (!hydraUserId.empty()) {
      st.TouchUser(hydraUserId);
      changed |= SetIfDifferent(st.users[hydraUserId].userIdentity, hydraUserId);
      changed |= SetIfDifferent(st.users[hydraUserId].hydraKernelSessionId, hydraKernelSessionId);
      if (sc.hydraUserId.empty())
         changed |= SetIfDifferent(sc.hydraUserId, hydraUserId);

      auto it = sc.hydraUsers.find(hydraUserId);
      if (it == sc.hydraUsers.end()) {
         sc.hydraUsers[hydraUserId] = hydraKernelSessionId;
         changed = true;
      } else {
         changed |= SetIfDifferent(it->second, hydraKernelSessionId);
      }
   }

   changed |= SetIfDifferent(sc.hydraKernelSessionId, hydraKernelSessionId);
   changed |= SetIfDifferent(sc.dataCenterId, dataCenterId);

   return changed;
}

static bool ApplySessionEventMemberContexts(LiveState& st, SCSessionState& sc, const Json& j)
{
   bool changed = false;

   if (j.is_object()) {
      auto it = j.find("serverUserContext");
      if (it != j.end() && it->is_object()) {
         const Json& suc = *it;
         const std::string hydraUserId = JsonGetString(suc, { "userContext", "data", "userIdentity" });
         const std::string hydraKernelSessionId = JsonGetString(suc, { "userContext", "data", "kernelSessionId" });
         changed |= AttachHydraContextToSC(st, sc, hydraUserId, hydraKernelSessionId);
      }

      for (const auto& kv : j.items()) {
         changed |= ApplySessionEventMemberContexts(st, sc, kv.value());
      }
   } else if (j.is_array()) {
      for (const auto& item : j) {
         changed |= ApplySessionEventMemberContexts(st, sc, item);
      }
   }

   return changed;
}

static bool ApplyServerFactsContext(SCSessionState& sc, const Json& p)
{
   bool changed = false;

   changed |= SetIfDifferent(sc.serverFactSessionId, JsonGetString(p, { "header", "factSessionId" }));

   if (const auto* ctx = NodeAt(p, Json::json_pointer("/header/context")); ctx && ctx->is_array()) {
      for (const auto& e : *ctx) {
         if (!e.is_object())
            continue;

         const std::string key = GetStr(e, "propertyName", "");
         const std::string value = GetStr(e, "propertyValue", "");
         if (key == "CLIENT_VERSION") {
            changed |= SetIfDifferent(sc.clientVersion, value);
         } else if (key == "KERNEL_SESSION_ID") {
            changed |= SetIfDifferent(sc.serverContextKernelSessionId, value);
         } else if (key == "DEDICATED_SERVER_ID") {
            // Despite the name in server facts, observed value is the SC/kernel session id.
            changed |= SetIfDifferent(sc.serverFactSessionId, value);
         }
      }
   }

   return changed;
}

static bool RemoveUsersFromAllMMSessions(LiveState& st, const std::unordered_set<std::string>& userIds)
{
   if (userIds.empty())
      return false;

   bool changed = false;
   for (auto& skv : st.sessions) {
      auto& sess = skv.second;
      for (const std::string& uid : userIds) {
         if (sess.members.erase(uid) > 0)
            changed = true;
      }
   }

   return changed;
}

bool StartServerController::HandleDedicatedServerHandshake(SdkPacket& u)
{
   const std::string serverId = u.payload.value("serverId", "");
   if (serverId.empty())
      return false; // HandshakeRequest can be empty

   st.ClearTombstone("server:" + serverId);
   if (!st.TouchServer(serverId))
      return false;
   auto& s = st.servers[serverId];

   bool changed = false;
   const bool modern = u.payload.value("isModernApi", false);
   if (s.isModernApi != modern) {
      s.isModernApi = modern;
      changed = true;
   }

   if (s.nodeKind != NodeKind::HeatedDSServer) {
      s.nodeKind = NodeKind::HeatedDSServer;
      changed = true;
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

bool StartServerController::HandleDedicatedServerSessionInfo(SdkPacket& u)
{
   if (u.reqNameId == HYDRA_API_DEDICATEDSERVERS_DSDSMCOMMUNICATION_GETSERVERSESSIONINFOREQUEST) {
      const std::string serverId = u.payload.value("serverId", "");
      if (serverId.empty())
         return false;

      const bool isNewServer = (st.servers.find(serverId) == st.servers.end());
      if (!st.TouchServer(serverId))
         return false;
      standaloneCorr.pendingGetServerSessionInfoServerId = serverId;
      return isNewServer;
   }

   const std::string serverId = standaloneCorr.pendingGetServerSessionInfoServerId;
   if (serverId.empty()) {
      // TODO: if server session info responses become interleaved, correlate
      // requests/responses with a queue or stronger request id instead.
      return false;
   }

   if (!st.TouchServer(serverId))
      return false;
   auto& s = st.servers[serverId];

   bool changed = false;

   const std::string refreshAfter = u.payload.value("refreshAfter", "");
   if (s.refreshAfter != refreshAfter) {
      s.refreshAfter = refreshAfter;
      changed = true;
   }

   if (u.payload.contains("sessionInfo") && !u.payload["sessionInfo"].is_null()) {
      const Json& sessionInfo = u.payload["sessionInfo"];
      changed |= SetBoolIfDifferent(s.hasSessionInfo, true);
      changed |= SetIfDifferent(s.reportedIp, JsonGetString(sessionInfo, { "reportedInfo", "ip" }));
      changed |= SetIfDifferent(s.reportedIpv6, JsonGetString(sessionInfo, { "reportedInfo", "ipv6" }));
      changed |= SetIfDifferent(s.authEndpointService, JsonGetString(sessionInfo, { "authEndpoint", "serviceInfo", "name" }));
      changed |= SetIfDifferent(s.authEndpointIp, JsonGetString(sessionInfo, { "authEndpoint", "ip" }));
      changed |= SetIfDifferent(s.authEndpointPort, JsonGetString(sessionInfo, { "authEndpoint", "port" }));

      changed |= SetIfDifferent(standaloneCorr.pendingSCActivationServerId, serverId);
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

bool StartServerController::HandleSCCreateSessionRequest(SdkPacket& u)
{
   const auto& p = u.payload;

   PendingSessionControlCreate ctx;
   ctx.hydraUserId = ExtractUserIdentity(p);
   ctx.hydraKernelSessionId = ExtractHydraKernelSessionId(p);
   ctx.dataCenterId = JsonGetString(p, { "dataCenterId" });
   ctx.clientVersion = JsonGetString(p, { "clientVersion" });
   ctx.serverData = JsonGetString(p, { "serverData" });

   if (ctx.hydraUserId.empty() && ctx.hydraKernelSessionId.empty())
      return false;

   if (!ctx.hydraUserId.empty()) {
      st.TouchUser(ctx.hydraUserId);
      SetIfDifferent(st.users[ctx.hydraUserId].hydraKernelSessionId, ctx.hydraKernelSessionId);
   }

   standaloneCorr.pendingSCCreate = std::move(ctx);
   return false; // correlation only until CreateSessionResponse gives gameSessionId
}

bool StartServerController::HandleSCCreateSessionResponse(SdkPacket& u)
{
   const auto& p = u.payload;
   const std::string scid = JsonGetString(p, { "gameSessionId" });
   if (scid.empty())
      return false;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   st.ClearTombstone("scsession:" + scid);
   if (!st.TouchSCSession(scid))
      return false;
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);

   const auto& ctx = standaloneCorr.pendingSCCreate;
   changed |= AttachHydraContextToSC(st, sc, ctx.hydraUserId, ctx.hydraKernelSessionId, ctx.dataCenterId);
   changed |= SetIfDifferent(sc.clientVersion, ctx.clientVersion);
   changed |= SetIfDifferent(sc.serverData, ctx.serverData);

   standaloneCorr.pendingSCCreate = {};
   return changed;
}

bool StartServerController::HandleSCGetServerInfoRequest(SdkPacket& u)
{
   const auto& p = u.payload;
   const std::string scid = JsonGetString(p, { "gameSessionId" });
   if (scid.empty())
      return false;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   if (!st.TouchSCSession(scid))
      return false;
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);
   changed |= AttachHydraContextToSC(st, sc,
      JsonGetString(p, { "userContext", "data", "userIdentity" }),
      JsonGetString(p, { "userContext", "data", "kernelSessionId" }));
   standaloneCorr.pendingGetServerInfoSCSessionId = scid;

   return changed;
}

bool StartServerController::HandleSCGetServerInfoResponse(SdkPacket& u)
{
   const std::string scid = standaloneCorr.pendingGetServerInfoSCSessionId;
   if (scid.empty())
      return false;

   if (!st.TouchSCSession(scid)) {
      standaloneCorr.pendingGetServerInfoSCSessionId.clear();
      return false;
   }
   auto& sc = st.scSessions[scid];

   bool changed = false;
   changed |= SetIfDifferent(sc.refreshAfterSeconds, JsonGetString(u.payload, { "refreshAfterSeconds" }));
   changed |= SetIfDifferent(sc.acceptStatus, JsonGetString(u.payload, { "acceptClientResult", "status" }));

   if (auto it = u.payload.find("acceptClientResult"); it != u.payload.end() && it->is_object()) {
      if (auto itInfo = it->find("serverInfo"); itInfo != it->end() && itInfo->is_object()) {
         changed |= ApplyServerInfoToSC(sc, *itInfo);
         changed |= ApplyServerInfoToLinkedServer(st, sc);
      }
   }

   standaloneCorr.pendingGetServerInfoSCSessionId.clear();
   return changed;
}

bool StartServerController::HandleSCGetSessionEventsRequest(SdkPacket& u)
{
   const std::string scid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (scid.empty())
      return false;

   standaloneCorr.pendingGetSessionEventsSCSessionId = scid;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   if (!st.TouchSCSession(scid))
      return false;
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);
   return changed;
}

bool StartServerController::HandleSCGetSessionEventsResponse(SdkPacket& u)
{
   const std::string scid = standaloneCorr.pendingGetSessionEventsSCSessionId;
   if (scid.empty())
      return false;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   if (!st.TouchSCSession(scid)) {
      standaloneCorr.pendingGetSessionEventsSCSessionId.clear();
      return false;
   }
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= ApplySessionEventMemberContexts(st, sc, u.payload);
   changed |= SetIfDifferent(sc.lastSessionMemberEventId,
      JsonGetString(u.payload, { "sessionMemberEvents", "lastEventId" }));

   standaloneCorr.pendingGetSessionEventsSCSessionId.clear();
   return changed;
}

bool StartServerController::HandleSCPrepareActivateSessionResponse(SdkPacket& u)
{
   const std::string scid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (scid.empty())
      return false;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   if (!st.TouchSCSession(scid))
      return false;
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);
   changed |= SetIfDifferent(sc.serverData, JsonGetString(u.payload, { "serverData" }));

   const std::string serverId = standaloneCorr.pendingSCActivationServerId;
   if (!serverId.empty()) {
      if (!st.TouchServer(serverId))
         return changed;
      auto& server = st.servers[serverId];
      changed |= SetIfDifferent(server.scSessionId, scid);
      changed |= SetIfDifferent(sc.serverId, serverId);
      standaloneCorr.pendingSCActivationServerId.clear();
   } else {
      // TODO: if activation responses become interleaved, correlate the
      // pending server with a queue/map keyed by stronger request evidence.
   }

   return changed;
}

bool StartServerController::HandleSCActivateSession3Request(SdkPacket& u)
{
   const std::string scid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (scid.empty())
      return false;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   if (!st.TouchSCSession(scid))
      return false;
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);

   if (auto it = u.payload.find("serverInfo"); it != u.payload.end() && it->is_object()) {
      changed |= ApplyServerInfoToSC(sc, *it);
      changed |= ApplyServerInfoToLinkedServer(st, sc);
   }

   return changed;
}

bool StartServerController::HandleSCProcessSessionMemberEventsRequest(SdkPacket& u)
{
   const std::string scid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (scid.empty())
      return false;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   if (!st.TouchSCSession(scid))
      return false;
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);

   if (auto it = u.payload.find("list"); it != u.payload.end() && it->is_array()) {
      for (const auto& item : *it) {
         changed |= SetIfDifferent(sc.acceptStatus, JsonGetString(item, { "data", "status" }));
      }
   }

   return changed;
}

bool StartServerController::HandleSCFinishSessionRequest(SdkPacket& u)
{
   const std::string scid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (scid.empty())
      return false;

   standaloneCorr.pendingFinishSessionSCSessionIds.push_back(scid);

   bool changed = false;
   auto itSC = st.scSessions.find(scid);
   if (itSC != st.scSessions.end()) {
      SCSessionState& sc = itSC->second;
      changed |= !sc.hydraUserId.empty();
      changed |= !sc.hydraKernelSessionId.empty();
      changed |= !sc.hydraUsers.empty();
      sc.hydraUserId.clear();
      sc.hydraKernelSessionId.clear();
      sc.hydraUsers.clear();
   }

   std::unordered_set<std::string> affectedUsers;
   for (const auto& pkv : st.parties) {
      const PartyState& party = pkv.second;
      if (party.gameSessionId != scid)
         continue;

      for (const auto& mkv : party.members)
         affectedUsers.insert(mkv.first);
   }

   changed |= RemoveUsersFromAllMMSessions(st, affectedUsers);
   return changed;
}

bool StartServerController::HandleSCFinishSessionResponse(SdkPacket& u)
{
   (void)u;

   if (standaloneCorr.pendingFinishSessionSCSessionIds.empty())
      return false;

   const std::string scid = standaloneCorr.pendingFinishSessionSCSessionIds.front();
   standaloneCorr.pendingFinishSessionSCSessionIds.pop_front();

   std::string serverId;
   auto itSC = st.scSessions.find(scid);
   if (itSC != st.scSessions.end()) {
      serverId = itSC->second.serverId;
   }

   if (serverId.empty()) {
      for (const auto& skv : st.servers) {
         if (skv.second.scSessionId == scid) {
            serverId = skv.first;
            break;
         }
      }
   }

   bool changed = false;
   changed |= st.RemoveSCSession(scid);
   if (!serverId.empty())
      changed |= st.RemoveServer(serverId);

   return changed;
}

bool StartServerController::HandleFactsWriteBinaryPackServer(SdkPacket& u)
{
   std::string scid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (scid.empty())
      scid = JsonGetString(u.payload, { "header", "factSessionId" });
   if (scid.empty())
      return false;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   if (!st.TouchSCSession(scid))
      return false;
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);
   changed |= ApplyServerFactsContext(sc, u.payload);

   return changed;
}

bool StartServerController::HandleDedicatedServerSetGameSessionTags(SdkPacket& u)
{
   const std::string scid = JsonGetString(u.payload, { "context", "data", "kernelSessionId" });
   if (scid.empty())
      return false;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   if (!st.TouchSCSession(scid))
      return false;
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);

   if (!sc.serverId.empty()) {
      if (!st.TouchServer(sc.serverId))
         return changed;
      changed |= SetIfDifferent(st.servers[sc.serverId].serverState, JsonGetString(u.payload, { "state" }));
   }

   return changed;
}

bool StartServerController::HandleGetStandaloneSignInCodeRequest(SdkPacket& u)
{
   const std::string uid = JsonGetString(u.payload, { "userContext", "data", "userIdentity" });
   if (uid.empty())
      return false;

   standaloneCorr.pendingGetCodeUserId = uid;
   return false; // correlation only, no graph change yet
}

bool StartServerController::HandleGetStandaloneSignInCodeResponse(SdkPacket& u)
{
   const std::string code = JsonGetString(u.payload, { "standaloneCode" });
   if (code.empty() || standaloneCorr.pendingGetCodeUserId.empty())
      return false;

   standaloneCorr.codeToHydraUserId[code] = standaloneCorr.pendingGetCodeUserId;
   standaloneCorr.pendingGetCodeUserId.clear();
   return false; // still correlation only
}

bool StartServerController::HandleSignInStandaloneCodeRequest(SdkPacket& u)
{
   const std::string code = JsonGetString(u.payload, { "standaloneCode" });
   if (code.empty())
      return false;

   auto it = standaloneCorr.codeToHydraUserId.find(code);
   standaloneCorr.pendingSignInUserId =
      (it != standaloneCorr.codeToHydraUserId.end()) ? it->second : std::string{};

   return false;
}

bool StartServerController::HandleSignInStandaloneCodeResponse(SdkPacket& u)
{
   const std::string serverToken = JsonGetString(u.payload, { "serverToken" });
   if (serverToken.empty() || standaloneCorr.pendingSignInUserId.empty())
      return false;

   standaloneCorr.serverTokenToHydraUserId[serverToken] = standaloneCorr.pendingSignInUserId;
   standaloneCorr.pendingSignInUserId.clear();
   return false;
}

bool StartServerController::HandleStandaloneHeartbeatServerRequest(SdkPacket& u)
{
   const std::string sid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (sid.empty())
      return false;

   st.TouchServer(sid);
   auto& s = st.servers[sid];

   bool changed = false;

   if (s.serverId != sid) {
      s.serverId = sid;
      changed = true;
   }

   if (s.nodeKind != NodeKind::StandaloneServer) {
      s.nodeKind = NodeKind::StandaloneServer;
      changed = true;
   }

   const std::string serverName = JsonGetString(u.payload, { "update", "data", "serverName", "data" });
   if (!serverName.empty() && s.serverName != serverName) {
      s.serverName = serverName;
      changed = true;
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

bool StartServerController::HandleStandaloneServerGetChallengesRequest(SdkPacket& u)
{
   const std::string sid = JsonGetString(u.payload, { "context", "data", "kernelSessionId" });
   if (sid.empty())
      return false;

   st.TouchServer(sid);
   auto& s = st.servers[sid];

   bool changed = false;

   if (s.serverId != sid) {
      s.serverId = sid;
      changed = true;
   }

   if (s.nodeKind != NodeKind::StandaloneServer) {
      s.nodeKind = NodeKind::StandaloneServer;
      changed = true;
   }

   auto it = u.payload.find("userIds");
   if (it != u.payload.end() && it->is_array() && !it->empty() && (*it)[0].is_string()) {
      const std::string uid = (*it)[0].get<std::string>();
      if (!uid.empty() && s.hydraUserId != uid) {
         s.hydraUserId = uid;
         changed = true;
      }
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

bool StartServerController::HandleCreateStandaloneServerRequest(SdkPacket& u)
{
   const std::string serverToken = JsonGetString(u.payload, { "serverToken" });
   if (serverToken.empty())
      return false;

   auto it = standaloneCorr.serverTokenToHydraUserId.find(serverToken);
   if (it == standaloneCorr.serverTokenToHydraUserId.end())
      return false;

   PendingStandaloneCreate ctx;
   ctx.hydraUserId = it->second;
   ctx.serverName = JsonGetString(u.payload, { "createData", "serverName", "data" });

   standaloneCorr.pendingCreates.push_back(std::move(ctx));
   return false;
}

bool StartServerController::HandleCreateStandaloneServerResponse(SdkPacket& u)
{
   const std::string sid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (sid.empty())
      return false;

   st.TouchServer(sid);
   auto& s = st.servers[sid];

   bool changed = false;

   if (s.serverId != sid) {
      s.serverId = sid;
      changed = true;
   }

   if (s.nodeKind != NodeKind::StandaloneServer) {
      s.nodeKind = NodeKind::StandaloneServer;
      changed = true;
   }

   if (!standaloneCorr.pendingCreates.empty()) {
      PendingStandaloneCreate ctx = std::move(standaloneCorr.pendingCreates.front());
      standaloneCorr.pendingCreates.pop_front();

      if (s.hydraUserId != ctx.hydraUserId) {
         s.hydraUserId = ctx.hydraUserId;
         changed = true;
      }

      if (s.serverName != ctx.serverName) {
         s.serverName = ctx.serverName;
         changed = true;
      }
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

} // namespace Sample::UI::Controllers
