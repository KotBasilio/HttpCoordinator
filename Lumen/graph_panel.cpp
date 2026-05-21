#include "graph_panel.h"

#include <algorithm> // std::clamp
#include <cmath> // std::sqrt, std::abs

#pragma message("graph_panel.cpp REV: LODs v0.1")

GraphPanel::GraphPanel(GraphViewState& _view, GraphModel& _model, Sample::Tex::TextureManager& _tex)
   : view(_view)
   , model(_model)
   , tex(_tex)
{
   EnsureModelPresence();
}

static ImU32 ScaleAlpha(ImU32 col, float factor)
{
   const int a = (int)((col >> 24) & 0xFF);
   int na = (int)(a * factor);
   na = std::clamp(na, 0, 255);
   return (col & 0x00FFFFFFu) | ((ImU32)na << 24);
}

static bool PointInRect(const Vec2f& p, const Vec2f& a, const Vec2f& b)
{
   return (p.x >= a.x && p.y >= a.y && p.x < b.x && p.y < b.y);
}

static bool IsMouseHoveringRect(const Vec2f& a, const Vec2f& b)
{
   const Vec2f m = ImGui::GetIO().MousePos;
   return PointInRect(m, a, b);
}

static void DrawArrowhead(ImDrawList* dl, const Vec2f& tip, const Vec2f& dir_norm, float size, ImU32 col)
{
   // Arrow tip pointing along dir_norm (must be normalized).
   // Build a small isosceles triangle.
   const Vec2f n = dir_norm;
   const Vec2f perp = Vec2f(-n.y, n.x);

   const Vec2f base = Vec2f(tip.x - n.x * size + 5.f, tip.y - n.y * size);
   const Vec2f p1 = Vec2f(base.x + perp.x * (size * 0.6f), base.y + perp.y * (size * 0.6f));
   const Vec2f p2 = Vec2f(base.x - perp.x * (size * 0.6f), base.y - perp.y * (size * 0.6f));

   dl->AddTriangleFilled(tip.AsIm(), p1.AsIm(), p2.AsIm(), col);
}

static Vec2f NormalizeSafe(const Vec2f& v)
{
   const float len2 = v.x * v.x + v.y * v.y;
   if (len2 <= 1e-8f) return Vec2f(1.0f, 0.0f);
   const float inv = 1.0f / std::sqrt(len2);
   return Vec2f(v.x * inv, v.y * inv);
}

void GraphPanel::ProcessMouseCommands()
{
   // Only when the canvas item is hovered
   if (!ImGui::IsItemHovered()) {
      return;
   }

   ImGuiIO& io = ImGui::GetIO();

   // WHEEL ZOOM (cursor-centered, wheel doesn't conflict with click)
   // One notch = kZoomFactor, negative wheel zooms out.
   if (io.MouseWheel != 0.0f) {
      const float factor = std::pow(kZoomFactor, io.MouseWheel);
      ApplyZoomAtScreenPos(factor, io.MousePos);
   }

   // PAN with middle-mouse or right-mouse drag (only when canvas hovered/active)
   if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || 
       ImGui::IsMouseDragging(ImGuiMouseButton_Right,  0.0f)) {
      const Vec2f d = io.MouseDelta;
      view.pan.x += d.x;
      view.pan.y += d.y;
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
      return;
   }

   // Click selection (click on empty space clears selection)
   if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
      view.selected.node = hovered_id;
      return;
   }
}

void GraphPanel::RenderGrid(ImDrawList* dl)
{
   // draw a light grid
   const float grid_step = 64.0f * view.zoom;
   if (grid_step >= 8.0f) {
      // Start offsets so grid moves with pan
      const float x0 = std::fmod(view.pan.x, grid_step);
      const float y0 = std::fmod(view.pan.y, grid_step);

      const ImU32 col = IM_COL32(255, 255, 255, 18);

      for (float x = canvas_pos.x + x0; x < canvas_max.x; x += grid_step)
         dl->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_max.y), col);

      for (float y = canvas_pos.y + y0; y < canvas_max.y; y += grid_step)
         dl->AddLine(ImVec2(canvas_pos.x, y), ImVec2(canvas_max.x, y), col);
   }
}

// Uses right-mid of source to left-mid of destination.
// Bezier control points produce a nice left-to-right flow.
void GraphPanel::RenderLinks(ImDrawList* dl)
{
   // a spider visual: a bright selected node (as a body), plus links (as limbs)
   // over a web of dimmed other links -- looks like a spider.
   const NodeId sel = view.selected.node;
   const bool spider = view.dimUnrelatedLinks && (sel != 0);
   constexpr float kDim = 0.15f;

   // Policy: draw at most one arrowhead per target node to avoid alpha stacking blobs.
   std::vector<NodeId> drewIncomingArrow;
   drewIncomingArrow.reserve(model.nodes.size());
   auto needArrowTo = [&](const GraphLink& e) -> bool {
      // pre-req
      if (!e.arrow) {
         return false;
      }

      // spider mode, related link => skip none
      if (spider && (e.to == sel || e.from == sel)) {
         return true;
      }

      // linear scan; Optional future: use unordered_set if this becomes a bottleneck (unlikely)
      bool missing = std::find(drewIncomingArrow.begin(), drewIncomingArrow.end(), e.to) == drewIncomingArrow.end();
      return missing;
   };

   // for all links
   for (const GraphLink& e : model.links) {
      const GraphNode* a = model.Find(e.from);
      const GraphNode* b = model.Find(e.to);
      if (!a || !b) continue;

      const bool related = !spider || (e.from == sel) || (e.to == sel);
      const ImU32 col = related ? e.color : ScaleAlpha(e.color, kDim);

      // Node icon sizes in *graph space* (your model), scaled by view.zoom
      const float aW = (a->size.x > 0 ? a->size.x : iconSizeDef) * view.zoom;
      const float aH = (a->size.y > 0 ? a->size.y : iconSizeDef) * view.zoom;
      const float bW = (b->size.x > 0 ? b->size.x : iconSizeDef) * view.zoom;
      const float bH = (b->size.y > 0 ? b->size.y : iconSizeDef) * view.zoom;

      const Vec2f aMin = GraphToScreen(a->pos.AsIm());
      const Vec2f bMin = GraphToScreen(b->pos.AsIm());

      const Vec2f aSize(aW, aH);
      const Vec2f bSize(bW, bH);

      Vec2f p0 = e.AnchorFrom(aMin, aSize);
      Vec2f p3 = e.AnchorTo  (bMin, bSize);

      // Move p3 slightly away from node edge so it doesn't overlap the icon border.
      p3.x -= 3.f;
      
      // Bezier control points: push outward in X.
      // If nodes are close, keep it modest.
      const float dx = std::max(40.0f, std::abs(p3.x - p0.x) * 0.35f);
      const Vec2f p1(p0.x + dx, p0.y);
      const Vec2f p2(p3.x - dx, p3.y);

      // Thickness: scale a little with zoom but clamp so it stays readable.
      const float thicknessBase = std::clamp(2.0f * view.zoom, 1.5f, 4.0f);
      const float thickness = related ? thicknessBase : std::max(1.0f, thicknessBase * 0.75f);

      // Draw curve/line
      if (e.style == LinkStyle::Straight)
         dl->AddLine(p0.AsIm(), p3.AsIm(), col, thickness);
      else
         dl->AddBezierCubic(p0.AsIm(), p1.AsIm(), p2.AsIm(), p3.AsIm(), col, thickness);

      // Arrow on destination end only (right end of the link = the destination side)
      // We approximate the tangent near p3 using the last segment direction.
      if (needArrowTo(e)) {
         const Vec2f tangent = Vec2f(p3.x - p2.x, p3.y - p2.y);
         const Vec2f dir = NormalizeSafe(tangent);
         const float arrowSize = std::clamp(12.0f * view.zoom, 10.0f, 16.0f);

         // Move the arrow tip slightly to the right so it covers the end of the line nicely.
         const float tipBackoff = 2.0f;// no zoom
         const Vec2f tip = Vec2f(p3.x + dir.x * tipBackoff, p3.y);

         DrawArrowhead(dl, tip, dir, arrowSize, col);
         drewIncomingArrow.push_back(e.to);
      }

      // dotted endpoints: draw small circles at p0/p3
      if (e.dotted) {
         float r = std::clamp(3.0f * view.zoom, 2.0f, 6.0f);
         dl->AddCircleFilled(p0.AsIm(), r, col);
         dl->AddCircleFilled(p3.AsIm(), r, col);
      }

      // Optional future:
      // - hover highlight: compare against hovered link id
      // - arrow variation by link type
   }
}

void GraphPanel::RenderIcons(ImDrawList* dl)
{
   // --- MVP icon draw ---
   const Vec2f mouse = ImGui::GetIO().MousePos;
   const bool canvas_hovered = ImGui::IsItemHovered();
   hovered_id = 0;
   if (view.selected.node != 0) {
      view.selected.desiredPx = 0.0f;
      view.selected.drawW = 0.0f;
      view.selected.drawH = 0.0f;
      view.selected.assetID = 0;
      view.selected.lodPx = 0;
   }

   for (const GraphNode& n : model.nodes) {
      const float base_w = (n.size.x > 0 ? n.size.x : iconSizeDef);
      const float base_h = (n.size.y > 0 ? n.size.y : iconSizeDef);
      const float w = base_w * view.zoom;
      const float h = base_h * view.zoom;
      const float desiredPx = std::max(w, h);

      auto lod = tex.IconLodInfoForKind(n.kind, desiredPx);
      ImTextureID icon = tex.Access(lod.asset);
      if (!icon) continue;

      if (n.id == view.selected.node) {
         view.selected.desiredPx = desiredPx;
         view.selected.drawW = w;
         view.selected.drawH = h;
         view.selected.assetID = static_cast<int>(lod.asset);
         view.selected.lodPx = lod.variantPx;
      }

      const Vec2f p = GraphToScreen(n.pos.AsIm());
      const Vec2f p_max = Vec2f(p.x + w, p.y + h);

      // Hit-test only if inside canvas
      const bool in_canvas = PointInRect(mouse, canvas_pos, canvas_max);
      const bool hovered = canvas_hovered && in_canvas && PointInRect(mouse, p, p_max);

      if (hovered)
         hovered_id = n.id;

      // Draw icon
      dl->AddImage(icon, p.AsIm(), p_max.AsIm());

      // All texts under icon
      RenderITexts(w, h, dl, n);

      // Selection border around ICON ONLY
      if (view.selected.node == n.id) {
         const float rounding = 10.0f * view.zoom;
         const float thickness = std::clamp(3.0f * view.zoom, 1.0f, 4.0f);
         dl->AddRect(p.AsIm(), p_max.AsIm(), IM_COL32(0, 200, 255, 255), rounding, 0, thickness);
      } else if (hovered) {
         // Subtle hover border
         const float rounding = 10.0f * view.zoom;
         dl->AddRect(p.AsIm(), p_max.AsIm(), IM_COL32(0, 200, 255, 90), rounding, 0, 2.0f);
      }
   }
}

void GraphPanel::RenderITexts(float w, float h, ImDrawList* dl, const GraphNode& n)
{
   // Screen rect of the icon
   const Vec2f p_min = GraphToScreen(n.pos.AsIm());
   const Vec2f p_max = Vec2f(p_min.x + w, p_min.y + h);

   // Label padding under icon (screen pixels, clamp optional)
   const float labelPad = std::clamp(4.0f * view.zoom, 2.0f, 10.0f);

   // Measure text in screen space (no scaling, so this is stable)
   const char* label = n.title.c_str();
   const Vec2f textSize = ImGui::CalcTextSize(label);

   // screen-space text width exceeds 3x icon screen size -> hide the label.
   if (textSize.x >= 3.0f * w) {
      return;
   }

   // Center-align text under icon
   const float iconCenterX = (p_min.x + p_max.x) * 0.5f;
   Vec2f textPos(iconCenterX - textSize.x * 0.5f, p_max.y + labelPad);

   // avoid blurry half-pixels (crisper text)
   textPos.x = std::floor(textPos.x + 0.5f);
   textPos.y = std::floor(textPos.y + 0.5f);

   dl->AddText(textPos.AsIm(), IM_COL32(200, 220, 220, 255), label);

   // Future: optional subtitle, status icon, etc.
}

void GraphPanel::RenderModel(ImDrawList* dl)
{
   RenderLinks(dl);
   RenderIcons(dl);
}

void GraphPanel::RenderClipOverlay()
{
   // Hint overlay (outside clip is fine too)
   ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + 8, canvas_pos.y + 8));
   ImGui::TextUnformatted("Flow visualization canvas");
   auto pos = ImGui::GetCursorScreenPos();
   ImGui::SetCursorScreenPos(ImVec2(pos.x + 8, pos.y));
   ImGui::TextUnformatted("Middle-mouse drag: Pan");

}

void GraphPanel::Draw()
{
   // Reserve the remaining window area as the canvas
   // (This makes the canvas independent from ImGui layout flow.)
   canvas_pos = ImGui::GetCursorScreenPos();
   canvas_size = ImGui::GetContentRegionAvail();
   canvas_max = ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);

   // If tiny, bail.
   if (canvas_size.x < 10.0f || canvas_size.y < 10.0f) {
      ImGui::TextUnformatted("Canvas too small");
      ImGui::End();
      return;
   }

   // If the model was re-projected since last frame, drop zoom-undo
   if (model.projectionGeneration != lastSeenProjectionGeneration) {
      lastSeenProjectionGeneration = model.projectionGeneration;
      DisarmZoomUndo();
   }

   // Allow widgets drawn later (overlay) to receive hover/click even if they overlap the InvisibleButton.
   ImGui::SetNextItemAllowOverlap();

   // Make an invisible button that covers the whole canvas so we can capture mouse input.
   ImGui::InvisibleButton("##graph_canvas", canvas_size,
      ImGuiButtonFlags_MouseButtonLeft |
      ImGuiButtonFlags_MouseButtonRight |
      ImGuiButtonFlags_MouseButtonMiddle);

   const bool canvas_active = ImGui::IsItemActive();

   ImDrawList* dl = ImGui::GetWindowDrawList();

   // Background (simple dark fill)
   dl->AddRectFilled(canvas_pos, canvas_max, IM_COL32(20, 20, 20, 255));
   dl->AddRect(canvas_pos, canvas_max, IM_COL32(60, 60, 60, 255));

   // Clip all drawing to canvas
   dl->PushClipRect(canvas_pos, canvas_max, true);
   {
      RenderGrid(dl);
      RenderModel(dl);
      ProcessMouseCommands();
   }
   dl->PopClipRect();

   RenderClipOverlay();
   RenderZoomControlWidget();
}

