#include "graph_types.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

#pragma message("graph_types.cpp REV: LODs v0.2")

// -- Helpers --
static Vec2f AnchorRightMid(const Vec2f& node_min, const Vec2f& node_size)
{
   return Vec2f(node_min.x + node_size.x, node_min.y + node_size.y * 0.5f);
}

static Vec2f AnchorLeftMid(const Vec2f& node_min, const Vec2f& node_size)
{
   return Vec2f(node_min.x, node_min.y + node_size.y * 0.5f);
}

static Vec2f AnchorTopMid(const Vec2f& p, const Vec2f& s)
{
   return Vec2f(p.x + s.x * 0.5f, p.y);
}

static Vec2f AnchorBottomMid(const Vec2f& p, const Vec2f& s)
{
   return Vec2f(p.x + s.x * 0.5f, p.y + s.y);
}

// -- Implementation --
Vec2f GraphLink::AnchorForPort(GraphPort port, const Vec2f &p, const Vec2f& s) const
{
    switch (port) {
       case GraphPort::Left:   return AnchorLeftMid(p, s);
       case GraphPort::Right:  return AnchorRightMid(p, s);
       case GraphPort::Top:    return AnchorTopMid(p, s);
       case GraphPort::Bottom: return AnchorBottomMid(p, s);

       default:
           // Default graph convention: left -> right flow
           return AnchorRightMid(p, s);
    }
}

GraphLink::GraphLink()
   : color(IM_COL32(180, 180, 180, 255))
{
}

GraphLink::GraphLink(NodeId _from, NodeId _to)
   : GraphLink()
{
   from = _from;
   to = _to;
}

Vec2f GraphLink::AnchorFrom(const Vec2f& p, const Vec2f& s) const
{
   return AnchorForPort(fromPort, p, s);
}

Vec2f GraphLink::AnchorTo(const Vec2f& p, const Vec2f& s) const
{
   return AnchorForPort(toPort, p, s);
}

Vec2f::Vec2f(const ImVec2& v) : Vec2f(v.x, v.y) {}
ImVec2 Vec2f::AsIm() const { return ImVec2(x, y); }

// -- Helpers --

static bool NodeEchoesValue(const GraphNode& node, const std::string& value)
{
   if (value.empty()) {
      return false;
   }

   if (node.entityKey == value) {
      return true;
   }

   const size_t colon = node.entityKey.find(':');
   if (colon != std::string::npos && node.entityKey.substr(colon + 1) == value) {
      return true;
   }

   return node.title == value;
}

// -- Implementation --

const GraphNode* GraphModel::FindUniqueNodeEchoForValue(const std::string& value) const
{
   const GraphNode* found = nullptr;

   for (const GraphNode& node : nodes) {
      if (!NodeEchoesValue(node, value)) {
         continue;
      }

      // prefer users over everything else
      if (node.kind == NodeKind::User) {
         return &node;
      }

      // if we already have a match and it's not a user, then we have multiple echoes => no unique target
      if (found != nullptr) {
         return nullptr;
      }

      found = &node;
   }

   return found;
}

void GraphViewState::SelectNode(NodeId id)
{
   selected = SelectionState{};
   selected.node = id;
}
