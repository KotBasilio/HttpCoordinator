#pragma once
#include <cstddef>
#include "graph_types.h"
#include "texture_manager.h"

class UnitsPanel {
public:
   UnitsPanel(GraphViewState& view, GraphModel& model, Sample::Tex::TextureManager& tex);

   void Draw();

private:
   struct SectionSpec {
      const char* label = "";
      const NodeKind* kinds = nullptr;
      std::size_t kindCount = 0;
   };

   GraphViewState& view;
   GraphModel& model;
   Sample::Tex::TextureManager& tex;

   // UI helpers
   static ImU32 StatusColorFor(const GraphNode& n);

   bool DrawHeader(const char* label, int count);
   void DrawSection(const SectionSpec& section);
   void DrawNodeRow(const GraphNode& n);
};
