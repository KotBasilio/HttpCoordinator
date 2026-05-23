#pragma once
#include "graph_types.h"
#include "texture_manager.h"

class UnitsPanel {
public:
   UnitsPanel(GraphViewState& view, GraphModel& model, Sample::Tex::TextureManager& tex);

   void Draw();

private:
   GraphViewState& view;
   GraphModel& model;
   Sample::Tex::TextureManager& tex;

   // UI helpers
   static const char* KindLabel(NodeKind k);
   static ImU32 StatusColorFor(const GraphNode& n);

   bool DrawHeader(NodeKind kind, int count);
   void DrawKindSection(NodeKind kind);
   void DrawNodeRow(const GraphNode& n);
};
