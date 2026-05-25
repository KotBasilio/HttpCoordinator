// graph_types.h
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

using NodeId = uint64_t;

enum class NodeKind
{
   Unknown,
   DSSession,
   HeatedDSServer,
   SCSession,
   MMFlowSample,
   ProsSample,
   User,
   Party,
   MMSession,
   StandaloneServer,
   HydraSample,

   // Pure visual LOD keys. These are not graph entities by default, but they
   // share the same texture lookup path as graph node icons.
   ConnectorCross,
   ConnectorEnd,
   ConnectorStart,
   LocalSample,
   LocalServer,
   LocalUser,
   Offline,
   Online,
   PartyLeader,
   // extendable as needed
};

enum class GraphPort : uint8_t {
   Left,
   Right,
   Top,
   Bottom,
};

struct ImVec2;
struct Vec2f {
   float x;
   float y;

   Vec2f() : x(0), y(0) {}
   Vec2f(float x_, float y_) : x(x_), y(y_) {}
   Vec2f(const ImVec2& v);

   // Conversion to ImVec2
   ImVec2 AsIm() const;
};

struct GraphNode
{
   NodeId      id = 0;
   NodeKind    kind = NodeKind::Unknown;

   std::string title;      // e.g. "ServUser0050"
   std::string subtitle;   // e.g. "User" or short id
   Vec2f       pos;        // in "graph space" coordinates (not screen)
   Vec2f       size;       // node box size in graph space

   // Visual/style tags (optional but handy)
   uint32_t    colorBorder = 0;  // IM_COL32(...)
   uint32_t    colorFill   = 0;
   uint32_t    colorText   = 0;

   // Inspector payload key (userId/sessionId etc.)
   std::string entityKey;
   // facts, context pairs, etc.
   std::vector<std::pair<std::string, std::string>> kv;

   // Visual marker keys drawn as small overlays on the primary icon.
   // These are texture/LOD keys only; they do not create graph entities.
   std::vector<NodeKind> badges;
};

enum class LinkStyle : uint8_t
{
   Straight,
   Bezier,
};

struct GraphLink
{
   GraphLink();
   GraphLink(NodeId _from, NodeId _to);

   NodeId   from = 0;
   NodeId   to   = 0;

   GraphPort fromPort = GraphPort::Right;
   GraphPort toPort   = GraphPort::Left;

   LinkStyle style = LinkStyle::Bezier;

   // Optional visuals
   bool     arrow = true;
   bool     dotted = false;
   uint32_t color;

   Vec2f AnchorFrom(const Vec2f& p, const Vec2f& s) const;
   Vec2f AnchorTo  (const Vec2f& p, const Vec2f& s) const;
private:
   Vec2f AnchorForPort(GraphPort port, const Vec2f& p, const Vec2f& s) const;
};

struct GraphModel
{
   std::vector<GraphNode> nodes;
   std::vector<GraphLink> links;

   // Quick lookup (build once after changes)
   std::unordered_map<NodeId, size_t> indexById;

   void RebuildIndex()
   {
      indexById.clear();
      indexById.reserve(nodes.size());
      for (size_t i = 0; i < nodes.size(); ++i)
         indexById[nodes[i].id] = i;
   }

   const GraphNode* Find(NodeId id) const
   {
      auto it = indexById.find(id);
      return (it == indexById.end()) ? nullptr : &nodes[it->second];
   }

   const GraphNode* FindUniqueNodeEchoForValue(const std::string& value) const;

   // projection counter increments after every ProjectToGraph()
   uint64_t projectionGeneration = 0; 
};

// Shared selection spine across panels (Inventory, Graph, Inspector).
struct SelectionState {
   NodeId node = 0; // 0 = nothing selected

   // Icon Lod Debug
   float desiredPx = 0.0f;
   float drawW = 0.0f;
   float drawH = 0.0f;
   int assetID = 0; // AssetID of the icon chosen for the selected node
   int lodPx = 0;
};

// View state for a canvas: with pan, zoom
struct GraphViewState {
   Vec2f  pan{};                 // pixels
   float  zoom = 1.0f;
   SelectionState  selected{};   // selection spine

   // Intent for move selected item up/down in its visual order.
   // -1 = up, +1 = down, 0 = none (no-op clicks are fine).
   int pendingUpDownDelta = 0;

   // Spider mode: when a node is selected, dim unrelated links.
   bool dimUnrelatedLinks = true;

   void ClearSelection() { selected = SelectionState{}; }
   void SelectNode(NodeId id);
   bool IsSelected(NodeId id) const { return selected.node == id; }
   bool HasSelection() const { return selected.node != 0; }
};
