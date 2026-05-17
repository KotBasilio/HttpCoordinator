#include "ui/controllers/start_server_controller.h"
#include "ui/models/start_server_model.h"
#include <algorithm>  
#include <unordered_set>

#pragma message("projector.cpp REV: SC sessions v0.1")

namespace Sample::UI::Controllers 
{

static struct LayoutConfig
{
   // Columns defaults
   static constexpr float xBase = 80.0f;
   static constexpr float xHydra = xBase;
   static constexpr float xStep = 180.0f;
   static constexpr float xUser = xHydra + xStep;
   static constexpr float xServer = xHydra - xStep;

   // Rows
   static constexpr float yBase = 80.0f;
   static constexpr float yStep = 80.0f;
   static constexpr float minGap = 60.0f;

   // Columns dynamic defaults (can be updated based on visibility of columns)
   float xParty = xUser + xStep;
   float xMM = xParty + xStep;

   // sizes
   Vec2f s32{32, 32};
} lay;

static GraphNode& UpsertNode(GraphModel& g, NodeId id)
{
   // Linear search is fine at our current scale; we rebuild index at the end anyway.
   for (auto& n : g.nodes)
      if (n.id == id)
         return n;

   g.nodes.push_back({});
   auto& n = g.nodes.back();
   n.id = id;
   n.kind = NodeKind::Unknown;
   n.title.clear();
   n.subtitle.clear();
   n.entityKey.clear();
   n.kv.clear();
   n.pos = Vec2f(0, 0);
   n.size = Vec2f(160, 70);
   return n;
}

static void EnsureLink(GraphModel& g, NodeId from, NodeId to,
   GraphPort fromPort = GraphPort::Right, GraphPort toPort = GraphPort::Left)
{
   for (const auto& e : g.links)
      if (e.from == from && e.to == to)
         return;

   GraphLink L{};
   L.from = from;
   L.to = to;
   L.fromPort = fromPort;
   L.toPort = toPort;
   L.style = LinkStyle::Bezier;
   L.arrow = true;
   g.links.push_back(L);
}

static struct SaltVariants {
   uint32_t hydra = 0x48445241u; // "HDRA"
   uint32_t user = 0x55534552u;  // "USER"
   uint32_t mm = 0x4D4D5345u;    // "MMSE"
   uint32_t party = 0x50415254u; // "PART"
   uint32_t server = 0x44535256u; // "DSRV"
} salt;

static NodeId HashToNodeId(const std::string& s, uint32_t salt)
{
   // NodeId is graph id type; we keep it non-zero.
   size_t h = std::hash<std::string>{}(s);
   uint32_t v = (uint32_t)(h ^ (size_t)salt);
   v &= 0x7fffffffU;
   if (v == 0) v = 1;
   return (NodeId)v;
}

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

      n.kv.clear();
      n.kv.push_back({ "serverId", serverId });
      n.kv.push_back({ "kind", isStandalone ? "StandaloneServer" : "HeatedDSServer" });
      n.kv.push_back({ "isModernApi", s.isModernApi ? "true" : "false" });
      if (!s.refreshAfter.empty())
         n.kv.push_back({ "refreshAfter", s.refreshAfter });

      if (isStandalone) {
         if (!s.serverName.empty())
            n.title = s.serverName;

         if (!s.hydraUserId.empty()) {
            const NodeId hydraId = HashToNodeId(s.hydraUserId, salt.hydra);
            EnsureLink(graph, nodeId, hydraId);
         }
      }
   }
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

      const NodeId hydraId = HashToNodeId(uid, salt.hydra);
      const NodeId userId = HashToNodeId(uid, salt.user);

      // HydraSample
      {
         auto& n = UpsertNode(graph, hydraId);
         n.kind = NodeKind::HydraSample;
         n.title = "Hydra";
         n.subtitle = "entry";
         n.entityKey = "hydra:" + uid;
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
         n.kv = u.facts;
         n.pos = Vec2f(lay.xUser, y);
         n.size = lay.s32;
         std::sort(n.kv.begin(), n.kv.end(), [](const auto& a, const auto& b) {return a.first < b.first; });
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

      // user -> party membership links
      for (const auto& mkv : p.members) {
         const std::string& uid = mkv.first;
         const NodeId userNodeId = HashToNodeId(uid, salt.user);
         EnsureLink(graph, userNodeId, partyNodeId);
      }
   }
}

template<class ItemT, class OrderT, class TableT>
float LiveState::YforGroupAsCentroid(ItemT& item, const OrderT& order, const TableT& table) const
{
   // Step 1: centroid of members' user canvasY
   // users are projected already (see ProjectHydraUsers)
   float sumY = 0.0f;
   int   cnt = 0;
   for (const auto& mkv : item.members) {
      const std::string& uid = mkv.first;
      auto itU = users.find(uid);
      if (itU == users.end()) continue;
      sumY += itU->second.canvasY;
      ++cnt;
   }

   float y;
   if (cnt > 0) {
      y = sumY / (float)cnt;
   } else {
      const int idx = (item.viewIdx >= 0) ? item.viewIdx : 0;
      y = lay.yBase + (float)idx * lay.yStep;
   }

   // Step 2: overlap resolution (nudge down below earlier items)
   bool adjusted;
   do {
      adjusted = false;

      for (const auto& key : order) {
         auto it = table.find(key);
         if (it == table.end()) continue;

         const auto& other = it->second;
         if (other.viewIdx < 0) continue;
         if (other.viewIdx >= item.viewIdx) continue;

         if (std::fabs(y - other.canvasY) < lay.minGap) {
            y = other.canvasY + lay.yStep;
            adjusted = true;
         }
      }
   } while (adjusted);

   return y;
}
float LiveState::CalcYForNode(SessionState& s) const { return YforGroupAsCentroid(s, sessionOrder, sessions); }
float LiveState::CalcYForNode(PartyState& p)   const { return YforGroupAsCentroid(p, partyOrder, parties); }

void StartServerController::UpdateColumnsInLayout()
{
   // Sticky column enablement: once visible, never turns off this run
   bool partyVisibleNow = false;
   for (const auto& pkv : st.parties)
      if (!pkv.second.members.empty()) { partyVisibleNow = true; break; }
   if (partyVisibleNow) st.partyEverVisible = true;

   bool mmVisibleNow = false;
   for (const auto& skv : st.sessions)
      if (!skv.second.members.empty()) { mmVisibleNow = true; break; }
   if (mmVisibleNow) st.mmEverVisible = true;

   bool serversVisibleNow = false;
   for (const auto& dskv : st.servers)
      if (!dskv.second.serverId.empty()) { serversVisibleNow = true; break; }

   // Compute X positions from active columns
   float x = lay.xUser;
   x += lay.xStep;
   if (st.partyEverVisible) { lay.xParty = x; x += lay.xStep; }
   if (st.mmEverVisible) { lay.xMM = x; x += lay.xStep; }

   (void)serversVisibleNow; // intentional for now: semantic visibility, no layout shift yet
   // possible: if (serversVisibleNow) { lay.xServer = x; x += lay.xStep; }
}

static std::string FindSessionLeaderUid(const SessionState& s)
{
   std::string leader;
   int owners = 0;
   for (const auto& mkv : s.members) {
      if (mkv.second.isOwner) {
         leader = mkv.first;
         return leader;
      }
   }

   // no owner
   return {};
}

bool SessionState::SameMemberSet(const PartyState::MapT& other) const
{
   if (members.size() != other.size()) return false;
   for (const auto& kv : members) {
      if (other.find(kv.first) == other.end()) return false;
   }
   return true;
}

static void RemoveUserLinksToSession(GraphModel& g, NodeId sessNodeId,
   const std::unordered_set<NodeId>& userNodeIds)
{
   auto& links = g.links;
   links.erase(std::remove_if(links.begin(), links.end(),
      [&](const GraphLink& L) {
         return (L.to == sessNodeId) && (userNodeIds.find(L.from) != userNodeIds.end());
      }), links.end());
}

void StartServerController::ReduceLinksPartyOwnsSession()
{
   // if a party perfectly "owns" a session (same members + leader), 
   // hide the user->session links and add a party->session link for cleaner visualization
   auto& graph = mainModel->graph;

   // For each MM session, find a matching party by:
   // - leader is in party
   // - member sets equal
   for (const std::string& sid : st.sessionOrder) {
      auto itS = st.sessions.find(sid);
      if (itS == st.sessions.end()) continue;
      const SessionState& sess = itS->second;
      if (sess.members.empty()) continue;

      const std::string leaderUid = FindSessionLeaderUid(sess);
      if (leaderUid.empty()) continue;

      // Find party candidate: leader is a member AND member sets match
      const PartyState* bestParty = nullptr;
      for (const std::string& pid : st.partyOrder) {
         auto itP = st.parties.find(pid);
         if (itP == st.parties.end()) continue;
         const PartyState& party = itP->second;
         if (party.members.empty()) continue;

         if (party.members.find(leaderUid) == party.members.end())
            continue;

         if (!sess.SameMemberSet(party.members))
            continue;

         bestParty = &party;
         break;
      }

      if (!bestParty) continue;

      const NodeId sessNodeId = HashToNodeId(sid, salt.mm);
      const NodeId partyNodeId = HashToNodeId(bestParty->partyId, salt.party);

      // Drop user->session links for these members
      std::unordered_set<NodeId> userNodeIds;
      userNodeIds.reserve(sess.members.size());
      for (const auto& mkv : sess.members) {
         const NodeId userNodeId = HashToNodeId(mkv.first, salt.user);
         userNodeIds.insert(userNodeId);
      }
      RemoveUserLinksToSession(graph, sessNodeId, userNodeIds);

      // add Party->Session link
      EnsureLink(graph, partyNodeId, sessNodeId);
   }
}

void StartServerController::ProjectToGraph()
{
   // clear old
   auto& graph = mainModel->graph;
   graph.nodes.clear();
   graph.links.clear();
   graph.indexById.clear();
   st.ResetOwners();

   // columns are dynamic
   UpdateColumnsInLayout();

   // Create all nodes and links
   ProjectServers();
   ProjectHydraUsers();
   ProjectParties();
   ProjectMMSessions();

   // link reduction
   ReduceLinksPartyOwnsSession();

   graph.RebuildIndex();
   ++graph.projectionGeneration;
}

} // namespace Sample::UI::Controllers

