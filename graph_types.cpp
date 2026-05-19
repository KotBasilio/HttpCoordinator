#include "graph_types.h"
#include <imgui.h>

#pragma message("graph_types.cpp REV: rich kv v0.3")

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
