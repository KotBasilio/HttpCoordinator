#include "ui/controllers/start_server_controller.h"
#include "ui/controllers/coordinator_http_server.h"
#include "packet_json_helpers.h"

#include <utility>

#pragma message("servers_ingestion.cpp REV: SC sessions v0.0")

namespace Sample::UI::Controllers
{

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
   const std::string serverId = u.payload.value("serverId", "");
   if (serverId.empty())
      return false;

   st.TouchServer(serverId);
   auto& s = st.servers[serverId];

   bool changed = false;
   const std::string refreshAfter = u.payload.value("refreshAfter", "");
   if (s.refreshAfter != refreshAfter) {
      s.refreshAfter = refreshAfter;
      changed = true;
   }

   s.lastSeenTimeS = u.recv_time_s;
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
