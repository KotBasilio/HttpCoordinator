#include "ui/controllers/start_server_controller.h"
#include "ui/controllers/coordinator_http_server.h"
#include "packet_json_helpers.h"

#include <utility>

#pragma message("servers_ingestion.cpp REV: SC sessions v0.4")

namespace Sample::UI::Controllers
{

static bool SetIfDifferent(std::string& dst, const std::string& src)
{
   if (src.empty() || dst == src)
      return false;

   dst = src;
   return true;
}

static bool AttachHydraContextToSC(LiveState& st, SCSessionState& sc,
   const std::string& hydraUserId,
   const std::string& hydraKernelSessionId,
   const std::string& dataCenterId = {})
{
   bool changed = false;

   if (!hydraUserId.empty()) {
      st.TouchUser(hydraUserId);
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

bool StartServerController::HandleDedicatedServerHandshake(SdkPacket& u)
{
   const std::string serverId = u.payload.value("serverId", "");
   if (serverId.empty())
      return false; // HandshakeRequest can be empty

   st.TouchServer(serverId);
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
      st.TouchServer(serverId);
      standaloneCorr.pendingGetServerSessionInfoServerId = serverId;
      return isNewServer;
   }

   const std::string serverId = standaloneCorr.pendingGetServerSessionInfoServerId;
   if (serverId.empty()) {
      // TODO: if server session info responses become interleaved, correlate
      // requests/responses with a queue or stronger request id instead.
      return false;
   }

   st.TouchServer(serverId);
   auto& s = st.servers[serverId];

   bool changed = false;

   const std::string refreshAfter = u.payload.value("refreshAfter", "");
   if (s.refreshAfter != refreshAfter) {
      s.refreshAfter = refreshAfter;
      changed = true;
   }

   if (u.payload.contains("sessionInfo") && !u.payload["sessionInfo"].is_null()) {
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
   st.TouchSCSession(scid);
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
   st.TouchSCSession(scid);
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);
   changed |= AttachHydraContextToSC(st, sc,
      JsonGetString(p, { "userContext", "data", "userIdentity" }),
      JsonGetString(p, { "userContext", "data", "kernelSessionId" }));

   return changed;
}

bool StartServerController::HandleSCGetSessionEventsRequest(SdkPacket& u)
{
   const std::string scid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (scid.empty())
      return false;

   standaloneCorr.pendingGetSessionEventsSCSessionId = scid;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   st.TouchSCSession(scid);
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
   st.TouchSCSession(scid);
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= ApplySessionEventMemberContexts(st, sc, u.payload);

   standaloneCorr.pendingGetSessionEventsSCSessionId.clear();
   return changed;
}

bool StartServerController::HandleSCPrepareActivateSessionResponse(SdkPacket& u)
{
   const std::string scid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (scid.empty())
      return false;

   const bool isNewSCSession = (st.scSessions.find(scid) == st.scSessions.end());
   st.TouchSCSession(scid);
   auto& sc = st.scSessions[scid];

   bool changed = isNewSCSession;
   changed |= SetIfDifferent(sc.serverContextKernelSessionId, scid);
   changed |= SetIfDifferent(sc.serverData, JsonGetString(u.payload, { "serverData" }));

   const std::string serverId = standaloneCorr.pendingSCActivationServerId;
   if (!serverId.empty()) {
      st.TouchServer(serverId);
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
