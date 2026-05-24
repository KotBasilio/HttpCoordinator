#pragma once
#include "graph_types.h"
#include "texture_manager.h"

// responsible for drawing the Graph window content.
class GraphPanel {
public:
   GraphPanel(GraphViewState& view, GraphModel& model, Sample::Tex::TextureManager& tex);
   void Draw();

private:
   // bound refs
   GraphViewState& view;
   GraphModel& model;
   Sample::Tex::TextureManager& tex;

   // current state
   ImVec2 canvas_pos{};
   ImVec2 canvas_size{};
   ImVec2 canvas_max{};
   NodeId hovered_id = 0;
   uint64_t lastSeenProjectionGeneration = 0;
   bool mockModel = false;

   void RenderGrid (ImDrawList* dl);
   void RenderModel(ImDrawList* dl);
   void RenderMockAssetPreview(ImDrawList* dl);
   void RenderIcons(ImDrawList* dl);
   void RenderITexts(float w, float h, ImDrawList* dl, const GraphNode& n);
   void RenderLinks(ImDrawList* dl);
   void RenderClipOverlay();
   void HandleHoveredSelected(const Vec2f& p, const Vec2f& p_max, const GraphNode& n, const float w, const float h, ImDrawList* dl);

   void EnsureModelPresence();
   void ProcessMouseCommands();

   // Camera helpers
   ImVec2 CanvasCenter() const;
   ImVec2 ScreenToGraph(const ImVec2& s) const;
   ImVec2 GraphToScreen(const ImVec2& p) const;
   void   ApplyZoomAtScreenPos(float zoomFactor, const ImVec2& screenAnchor);

   // Zoom commands (wired to ZCW)
   void ZoomIn();
   void ZoomOut();
   void DisarmZoomUndo();
   void FitToScreen();
   void OnFitToScreenClick();
   void RenderZoomControlWidget();

   // Fit-to-screen undo latch
   struct FitToScreenUndo {
      bool  Armed = false;
      // previous camera state
      ImVec2 Pan = ImVec2(0, 0);
      float Zoom = 1.0f;
      // applied fit-to-screen camera state
      ImVec2 AppliedPan = ImVec2(0, 0);
      float AppliedZoom = 1.0f;
      ImVec2 AppliedCanvasSize = ImVec2(0, 0);
      void ConsiderDisarming(GraphViewState& view, const ImVec2& currentCanvasSize);
   } undoFTS;

   // consts
   static constexpr float kMinZoom = 0.10f;
   static constexpr float kMaxZoom = 4.00f;
   static constexpr float kZoomFactor = 1.15f;
   static constexpr float iconSizeDef = 48.f;
};
