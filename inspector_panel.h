#pragma once
#include "graph_types.h"
#include "texture_manager.h"

// Responsible for drawing the Inspector window content.
// Note: caller must already be inside ImGui::Begin("Inspector").
class InspectorPanel {
public:
   InspectorPanel(GraphViewState& view, GraphModel& model, Sample::Tex::TextureManager& tex);

   void Draw();

private:
   GraphViewState& view;
   GraphModel& model;
   Sample::Tex::TextureManager& tex;

   void DrawEmpty();
   void DrawNotFound(NodeId id);
   void DrawNode(const GraphNode& n);
   void DrawUpDownButtons(const GraphNode& n, const ImVec2& fieldsStart);
   void DrawNodeKeys(const GraphNode& n);
   void DrawMaybeClickableKvValue(const std::string& value);
   void DrawKvCopyButton(int rowIndex, const std::string& key, const std::string& value);
   void DrawLinksOut(NodeId selectedId);
   void DrawClickableLink(const GraphLink& e, NodeId idDest);
   void DrawLinksIn(NodeId selectedId);

   static const char* ToString(NodeKind k);
};
