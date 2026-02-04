// graph_types.h
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <imgui.h>

#pragma message("start_server_controller.cpp REV: Graph nodes v0.2")

using NodeId = uint64_t;

enum class NodeKind : uint8_t
{
   Unknown,
   DSSession,
   HeatedDSServer,
   SCSession,
   MMFlowSample,
   User,
   Party,
   MMSession,
   StandaloneServer,
   HydraSample,
   // add as needed
};

enum class GraphPort : uint8_t {
   Left,
   Right,
   Top,
   Bottom,
};

struct GraphNode
{
   NodeId      id = 0;
   NodeKind    kind = NodeKind::Unknown;

   std::string title;      // e.g. "ServUser0050"
   std::string subtitle;   // e.g. "User" or short id
   ImVec2      pos;        // in "graph space" coordinates (not screen)
   ImVec2      size;       // node box size in graph space

   // Visual/style tags (optional but handy)
   uint32_t    colorBorder = 0;  // IM_COL32(...)
   uint32_t    colorFill   = 0;
   uint32_t    colorText   = 0;

   // Inspector payload key (points to some domain entity id)
   std::string entityKey;  // e.g. userId/sessionId etc.
};

enum class LinkStyle : uint8_t
{
   Straight,
   Bezier,
};

struct GraphLink
{
   NodeId   from = 0;
   NodeId   to   = 0;

   GraphPort fromPort = GraphPort::Right;
   GraphPort toPort   = GraphPort::Left;

   LinkStyle style = LinkStyle::Bezier;

   // Optional visuals
   bool     arrow = true;
   bool     dotted = false;
   uint32_t color = IM_COL32(180, 180, 180, 255);

   ImVec2 AnchorFrom(const ImVec2& p, const ImVec2& s) const;
   ImVec2 AnchorTo  (const ImVec2& p, const ImVec2& s) const;
private:
   ImVec2 AnchorForPort(GraphPort port, const ImVec2& p, const ImVec2& s) const;
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

   GraphNode* Find(NodeId id)
   {
      auto it = indexById.find(id);
      return (it == indexById.end()) ? nullptr : &nodes[it->second];
   }
};

// Shared selection spine across panels (Inventory, Graph, Inspector).
struct SelectionState {
   NodeId node = 0; // 0 = nothing selected
};

// Minimal view state for a canvas: pan now, zoom later.
struct GraphViewState {
   ImVec2 pan = ImVec2(0.0f, 0.0f); // pixels
   float  zoom = 1.0f;
   SelectionState  selected{};      // selection spine
};

