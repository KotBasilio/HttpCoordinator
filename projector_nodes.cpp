#include "projector_internal.h"

#include <algorithm>

#pragma message("projector_nodes.cpp REV: hydra identity v0.1")

namespace Sample::UI::Controllers
{

void StartServerController::ProjectServers()
{
   auto& graph = mainModel->graph;
   int serverIdx = 0;

   for (const std::string& serverId : st.serverOrder) {
      auto it = st.servers.find(serverId);
      if (it == st.servers.end())
         continue;

      ServerState& s = it->second;
      s.viewIdx = serverIdx++;
      s.canvasY = lay.yBase + (float)s.viewIdx * lay.yStep;

      const NodeId nodeId = HashToNodeId(serverId, salt.server);
      std::string shortId = serverId.substr(0, std::min<size_t>(8, serverId.size()));

      auto& n = UpsertNode(graph, nodeId);
      bool isStandalone = (s.nodeKind == NodeKind::StandaloneServer);
      n.kind = s.nodeKind;
      n.title = isStandalone ? "Standalone" : "Heated DS";
      n.subtitle = shortId
         + (s.isModernApi ? " | modern" : "")
         + (!s.refreshAfter.empty() ? " | refresh=" + s.refreshAfter : "");
      n.entityKey = "server:" + serverId;
      n.pos = Vec2f(lay.xServer, s.canvasY);
      n.size = lay.s32;

      FillServerKv(n, s, serverId);

      if (!s.scSessionId.empty()) {
         const NodeId scNodeId = HashToNodeId(s.scSessionId, salt.sc);
         EnsureLink(graph, nodeId, scNodeId);
      }

      if (isStandalone) {
         if (!s.serverName.empty())
            n.title = s.serverName;

         if (!s.hydraUserId.empty()) {
            const NodeId hydraId = HydraNodeIdForUser(st, s.hydraUserId);
            EnsureLink(graph, nodeId, hydraId);
         }
      }
   }
}

void StartServerController::ProjectSCSessions()
{
   auto& graph = mainModel->graph;
   int scIdx = 0;

   for (const std::string& scid : st.scSessionOrder) {
      auto it = st.scSessions.find(scid);
      if (it == st.scSessions.end())
         continue;

      SCSessionState& s = it->second;
      s.viewIdx = scIdx++;
      s.canvasY = lay.yBase + (float)s.viewIdx * lay.yStep;

      const NodeId scNodeId = HashToNodeId(scid, salt.sc);
      std::string shortId = scid.substr(0, std::min<size_t>(8, scid.size()));

      auto& n = UpsertNode(graph, scNodeId);
      n.kind = NodeKind::SCSession;
      n.title = "SC Session";
      n.subtitle = shortId;
      n.entityKey = "scsession:" + scid;
      n.pos = Vec2f(lay.xSCSession, s.canvasY);
      n.size = lay.s32;

      std::vector<std::string> hydraUserIds;
      hydraUserIds.reserve(s.hydraUsers.size());
      for (const auto& hkv : s.hydraUsers)
         hydraUserIds.push_back(hkv.first);
      std::sort(hydraUserIds.begin(), hydraUserIds.end());

      FillSCSessionKv(n, s, scid, hydraUserIds);

      if (!s.serverId.empty()) {
         const NodeId serverNodeId = HashToNodeId(s.serverId, salt.server);
         EnsureLink(graph, serverNodeId, scNodeId);
      }

      if (!s.hydraUserId.empty()) {
         const NodeId hydraNodeId = HydraNodeIdForUser(st, s.hydraUserId);
         EnsureLink(graph, scNodeId, hydraNodeId);
      }
      for (const std::string& hydraUserId : hydraUserIds) {
         const NodeId hydraNodeId = HydraNodeIdForUser(st, hydraUserId);
         EnsureLink(graph, scNodeId, hydraNodeId);
      }
   }

   // Server links come from dedicated-server session info.
   // Hydra links come from verified SessionControl request/response context.
}

void StartServerController::ProjectHydraUsers()
{
   // Create per-user HydraSample and User nodes
   auto& graph = mainModel->graph;
   for (size_t i = 0; i < st.userOrder.size(); ++i) {
      const std::string& uid = st.userOrder[i];
      auto it = st.users.find(uid);
      if (it == st.users.end()) continue;
      UserState& u = it->second;

      const float y = lay.yBase + (float)i * lay.yStep;
      u.canvasY = y;

      const std::string hydraIdentityKey = HydraIdentityKeyForUser(st, uid);
      const NodeId hydraId = HydraNodeIdForUser(st, uid);
      const NodeId userId = HashToNodeId(uid, salt.user);

      // HydraSample
      {
         auto& n = UpsertNode(graph, hydraId);
         n.kind = NodeKind::HydraSample;
         n.title = "Hydra";
         n.subtitle = "entry";
         n.entityKey = HydraEntityKeyForUser(st, uid);
         FillHydraKv(n, u, hydraIdentityKey);
         n.pos = Vec2f(lay.xHydra, y);
         n.size = lay.s32;
      }

      // User
      {
         std::string title = u.nickname.empty() ? uid.substr(0, 6) : u.nickname;
         std::string sub = u.online ? "ONLINE" : "OFFLINE";
         if (u.isOwnerAny) sub += "  W";

         auto& n = UpsertNode(graph, userId);
         n.kind = NodeKind::User;
         n.title = title;
         n.subtitle = sub;
         n.entityKey = "user:" + uid;
         FillUserKv(n, u);
         n.pos = Vec2f(lay.xUser, y);
         n.size = lay.s32;
      }

      // Link Hydra -> User (always)
      EnsureLink(graph, hydraId, userId);
   }
}

void StartServerController::ProjectMMSessions()
{
   // Create MMSession nodes (only keep sessions that currently have members)
   int sessIdx = 0;
   auto& graph = mainModel->graph;
   for (const std::string& sid : st.sessionOrder) {
      auto itS = st.sessions.find(sid);
      if (itS == st.sessions.end()) continue;
      SessionState& s = itS->second;

      // Hide empty sessions
      if (s.members.empty()) {
         s.viewIdx = -1; // reset index for when it becomes non-empty again
         continue;
      }

      const NodeId sessId = HashToNodeId(sid, salt.mm);
      s.viewIdx = sessIdx++;
      s.canvasY = st.CalcYForNode(s);

      std::string sub = s.mmState.empty() ? "MM" : s.mmState;
      sub += " | members=" + std::to_string(s.members.size());

      auto& n = UpsertNode(graph, sessId);
      n.kind = NodeKind::MMSession;
      n.title = "MM Session";
      n.subtitle = sub;
      n.entityKey = "mmsession:" + sid;
      n.pos = Vec2f(lay.xMM, s.canvasY);
      n.size = lay.s32;

      FillMMSessionKv(n, s, sid);

      // Membership links: User -> Session
      for (const auto& mkv : s.members) {
         const std::string& uid = mkv.first;
         const NodeId userNodeId = HashToNodeId(uid, salt.user);
         EnsureLink(graph, userNodeId, sessId);
      }
   }
}

void StartServerController::ProjectParties()
{
   // create Party nodes (only keep parties that currently have members)
   auto& graph = mainModel->graph;
   int partyIdx = 0;
   for (const std::string& pid : st.partyOrder) {
      auto itP = st.parties.find(pid);
      if (itP == st.parties.end()) continue;
      PartyState& p = itP->second;

      if (p.members.empty()) {
         p.viewIdx = -1;
         continue; // hide empty parties
      }

      p.viewIdx = partyIdx++;

      const NodeId partyNodeId = HashToNodeId(pid, salt.party);
      const float y = st.CalcYForNode(p);
      p.canvasY = y;

      std::string sub = "members=" + std::to_string(p.members.size());
      if (!p.joinCode.empty()) sub += " | code=" + p.joinCode;
      if (!p.leaderUid.empty()) sub += " | W";

      auto& n = UpsertNode(graph, partyNodeId);
      n.kind = NodeKind::Party;
      n.title = "Party";
      n.subtitle = sub;
      n.entityKey = "party:" + pid;
      n.pos = Vec2f(lay.xParty, y);
      n.size = lay.s32;

      FillPartyKv(n, p, pid);

      // user -> party membership links
      for (const auto& mkv : p.members) {
         const std::string& uid = mkv.first;
         const NodeId userNodeId = HashToNodeId(uid, salt.user);
         EnsureLink(graph, userNodeId, partyNodeId);
      }
   }
}

} // namespace Sample::UI::Controllers
