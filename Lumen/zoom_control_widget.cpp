#include "graph_panel.h"

#include <algorithm> // std::clamp
#include <cfloat> // FLT_MAX

static ImVec2 RoundPx(ImVec2 v)
{
   return ImVec2(std::floor(v.x + 0.5f), std::floor(v.y + 0.5f));
}

void GraphPanel::FitToScreenUndo::ConsiderDisarming(GraphViewState& view, const ImVec2& currentCanvasSize)
{
   if (!Armed)
      return;

   const ImVec2 cur = RoundPx(currentCanvasSize);

   // If camera changed since fit was applied OR viewport changed, disarm undo.
   const bool changed =
      (view.pan.x != AppliedPan.x) ||
      (view.pan.y != AppliedPan.y) ||
      (view.zoom != AppliedZoom) ||
      (cur.x != AppliedCanvasSize.x) ||
      (cur.y != AppliedCanvasSize.y);

   if (changed) {
      Armed = false;
   }
}

void GraphPanel::DisarmZoomUndo()
{
   undoFTS.Armed = false;
}

void GraphPanel::OnFitToScreenClick()
{
   // If armed and camera is still exactly the fit camera, treat as Undo.
   undoFTS.ConsiderDisarming(view, canvas_size);
   if (undoFTS.Armed) {
      view.pan = undoFTS.Pan;
      view.zoom = undoFTS.Zoom;
      DisarmZoomUndo();
      return;
   }

   // Otherwise: arm undo from current state, then do Fit.
   undoFTS.Pan = view.pan.AsIm();
   undoFTS.Zoom = view.zoom;
   FitToScreen();

   // Record the camera state *after* fit, so we can detect "no changes since fit"
   undoFTS.AppliedPan = view.pan.AsIm();
   undoFTS.AppliedZoom = view.zoom;
   undoFTS.AppliedCanvasSize = RoundPx(canvas_size);
   undoFTS.Armed = true;
}

void GraphPanel::FitToScreen()
{
   if (model.nodes.empty())
      return;

   // Compute graph-space bounds (same sizing logic as RenderIcons)
   ImVec2 bb_min(FLT_MAX, FLT_MAX);
   ImVec2 bb_max(-FLT_MAX, -FLT_MAX);

   for (const GraphNode& n : model.nodes) {
      const float w = (n.size.x > 0 ? n.size.x : iconSizeDef);
      const float h = (n.size.y > 0 ? n.size.y : iconSizeDef);

      bb_min.x = std::min(bb_min.x, n.pos.x);
      bb_min.y = std::min(bb_min.y, n.pos.y);
      bb_max.x = std::max(bb_max.x, n.pos.x + w);
      bb_max.y = std::max(bb_max.y, n.pos.y + h);
   }

   const float bb_w = std::max(1.0f, bb_max.x - bb_min.x);
   const float bb_h = std::max(1.0f, bb_max.y - bb_min.y);

   // Margin in screen pixels
   const float margin_px = 60.0f;
   const float avail_w = std::max(1.0f, canvas_size.x - margin_px * 2.0f);
   const float avail_h = std::max(1.0f, canvas_size.y - margin_px * 2.0f);

   // Choose zoom so bbox fits
   float z = std::min(avail_w / bb_w, avail_h / bb_h);

   // Clamp zoom to sane limits (tweak later if you want)
   z = std::clamp(z, kMinZoom, kMaxZoom);
   view.zoom = z;

   // Center bbox in the canvas:
   const ImVec2 bb_center((bb_min.x + bb_max.x) * 0.5f, (bb_min.y + bb_max.y) * 0.5f);
   const ImVec2 canvas_center(canvas_size.x * 0.5f, canvas_size.y * 0.5f);

   // pan is in screen pixels (relative to canvas origin)
   view.pan = ImVec2(
      canvas_center.x - bb_center.x * view.zoom,
      canvas_center.y - bb_center.y * view.zoom
   );
}

ImVec2 GraphPanel::ScreenToGraph(const ImVec2& s) const
{
   // Inverse of: screen = canvas_pos + view.pan + graph * view.zoom
   const ImVec2 local = ImVec2(s.x - canvas_pos.x - view.pan.x, s.y - canvas_pos.y - view.pan.y);
   const float invz = (view.zoom != 0.0f) ? (1.0f / view.zoom) : 1.0f;
   return ImVec2(local.x * invz, local.y * invz);
}

ImVec2 GraphPanel::GraphToScreen(const ImVec2& p) const
{
   // Graph space -> screen space
   // Screen = origin + pan + (graph * zoom)
   const ImVec2& canvas_origin = canvas_pos;
   return ImVec2(
      canvas_origin.x + view.pan.x + p.x * view.zoom,
      canvas_origin.y + view.pan.y + p.y * view.zoom
   );
}

void GraphPanel::ApplyZoomAtScreenPos(float zoomFactor, const ImVec2& screenAnchor)
{
   if (zoomFactor <= 0.0f)
      return;

   // Graph point under anchor before zoom
   const ImVec2 g_before = ScreenToGraph(screenAnchor);

   // Update zoom (clamped)
   float newZoom = view.zoom * zoomFactor;
   newZoom = std::clamp(newZoom, kMinZoom, kMaxZoom);

   // If clamping prevented change, no need to adjust pan
   if (newZoom == view.zoom)
      return;

   view.zoom = newZoom;

   // After zoom, adjust pan so the same graph point stays under the anchor
   const ImVec2 s_after = GraphToScreen(g_before);
   const ImVec2 delta = ImVec2(screenAnchor.x - s_after.x, screenAnchor.y - s_after.y);
   view.pan.x += delta.x;
   view.pan.y += delta.y;
}

ImVec2 GraphPanel::CanvasCenter() const
{
   return ImVec2(
      canvas_pos.x + canvas_size.x * 0.5f,
      canvas_pos.y + canvas_size.y * 0.5f);
}

void GraphPanel::ZoomIn()
{
   // Zoom around canvas center for now (stable + testable).
   // Later you can pass mouse pos for cursor-centered zoom.
   const ImVec2 anchor = CanvasCenter();
   ApplyZoomAtScreenPos(kZoomFactor, anchor);
}

void GraphPanel::ZoomOut()
{
   const ImVec2 anchor = CanvasCenter();
   ApplyZoomAtScreenPos(1.0f / kZoomFactor, anchor);
}

void GraphPanel::RenderZoomControlWidget()
{
   // Zoom-control widget (ZCW): bottom-right of the graph canvas.
   const float icon = 20.0f;
   const float pad = 8.0f;
   const float gap = 6.0f;

   const float box_w = icon + pad * 2.0f;
   const float box_h = (icon * 3.0f) + (gap * 2.0f) + (pad * 2.0f);

   // Position: inset from the canvas right/bottom edges.
   const ImVec2 box_pos(
      canvas_max.x - box_w - 12.0f,
      canvas_max.y - box_h - 12.0f
   );
   const ImVec2 box_max(box_pos.x + box_w, box_pos.y + box_h);

   ImDrawList* dl = ImGui::GetWindowDrawList();
   dl->AddRectFilled(box_pos, box_max, IM_COL32(30, 30, 30, 220), 6.0f);
   dl->AddRect(box_pos, box_max, IM_COL32(60, 60, 60, 255), 6.0f);

   ImGui::PushID("ZCW");

   // Make ImageButtons match our box math (no implicit frame padding)
   ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
   ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

   // Optional: flatter look like the design (no heavy ImGui button fill)
   ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 25));
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 45));

   // (1) Zoom in
   ImGui::SetCursorScreenPos(ImVec2(box_pos.x + pad, box_pos.y + pad));
   if (ImGui::ImageButton("##zplus",
      tex.Access(AssetID::IC_SCALE_PLUS_20_PX),
      ImVec2(icon, icon))) {
      ZoomIn();
   }
   if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
      ImGui::SetTooltip("Zoom in");
   }

   // (2) Zoom out
   ImGui::SetCursorScreenPos(ImVec2(box_pos.x + pad, box_pos.y + pad + (icon + gap) * 1.0f));
   if (ImGui::ImageButton("##zminus",
      tex.Access(AssetID::IC_SCALE_MINUS_20_PX),
      ImVec2(icon, icon))) {
      ZoomOut();
   }
   if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
      ImGui::SetTooltip("Zoom out");
   }

   // (3) Fit to screen
   ImGui::SetCursorScreenPos(ImVec2(box_pos.x + pad, box_pos.y + pad + (icon + gap) * 2.0f));
   if (ImGui::ImageButton("##zfit",
      tex.Access(AssetID::IC_SCALE_FIT_20_PX),
      ImVec2(icon, icon))) {
      OnFitToScreenClick();
   }
   if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
      ImGui::SetTooltip("Fit to screen");
   }

   ImGui::PopStyleColor(3);
   ImGui::PopStyleVar(2);

   ImGui::PopID();
}

