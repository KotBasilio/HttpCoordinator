#include "projector_internal.h"

#include "ui/models/start_server_model.h"

#include <functional>

namespace Sample::UI::Controllers
{

LayoutConfig lay;
SaltVariants salt;

GraphNode& UpsertNode(GraphModel& g, NodeId id)
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

void EnsureLink(GraphModel& g, NodeId from, NodeId to,
   GraphPort fromPort, GraphPort toPort)
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

NodeId HashToNodeId(const std::string& s, uint32_t salt)
{
   // NodeId is graph id type; we keep it non-zero.
   size_t h = std::hash<std::string>{}(s);
   uint32_t v = (uint32_t)(h ^ (size_t)salt);
   v &= 0x7fffffffU;
   if (v == 0) v = 1;
   return (NodeId)v;
}

std::string HydraIdentityKeyForUser(const LiveState& st, const std::string& userId)
{
   auto it = st.users.find(userId);
   if (it != st.users.end()) {
      const UserState& u = it->second;
      if (!u.runtimeSeanceId.empty())
         return u.runtimeSeanceId;
      if (!u.hydraKernelSessionId.empty())
         return u.hydraKernelSessionId;
   }

   return userId;
}

NodeId HydraNodeIdForUser(const LiveState& st, const std::string& userId)
{
   return HashToNodeId(HydraIdentityKeyForUser(st, userId), salt.hydra);
}

std::string HydraEntityKeyForUser(const LiveState& st, const std::string& userId)
{
   return "hydra:" + HydraIdentityKeyForUser(st, userId);
}

GraphNode* FindNode(GraphModel& g, NodeId id)
{
   for (auto& n : g.nodes)
      if (n.id == id)
         return &n;

   return nullptr;
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
   ProjectSCSessions();
   ProjectHydraUsers();
   ProjectParties();
   ProjectMMSessions();

   // link reduction
   ReduceLinksPartyOwnsSession();

   graph.RebuildIndex();
   ++graph.projectionGeneration;
}

} // namespace Sample::UI::Controllers
