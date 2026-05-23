#include "units_panel.h"
#include <imgui.h>
#include <algorithm> // std::sort
#include <cstdio> // snprintf

UnitsPanel::UnitsPanel(GraphViewState& view_, GraphModel& model_, Sample::Tex::TextureManager& tex_)
   : view(view_)
   , model(model_)
   , tex(tex_)
{
}

const char* UnitsPanel::KindLabel(NodeKind k)
{
   switch (k) {
      case NodeKind::User:             return "Users";
      case NodeKind::Party:            return "Parties";
      case NodeKind::SCSession:        return "SC Sessions";
      case NodeKind::MMSession:        return "MM Sessions";
      case NodeKind::MMFlowSample:     return "MM Flow Samples";
      case NodeKind::ProsSample:       return "Pros Samples";
      case NodeKind::DSSession:        return "DS Sessions";
      case NodeKind::HeatedDSServer:   return "Heated DS Servers";
      case NodeKind::StandaloneServer: return "Standalone Servers";
      case NodeKind::HydraSample:      return "Samples";
      case NodeKind::Unknown:          return "Other";
      default:                         return "Other";
   }
}

// Prototype "status dot" logic.
// Later: drive from real runtime state (online/starting/offline/etc).
ImU32 UnitsPanel::StatusColorFor(const GraphNode& n)
{
   // A simple readable mapping:
   // - Users/Parties = green (active)
   // - Servers = orange (service-ish)
   // - Sessions/flows = blue-ish (in progress)
   // - Unknown = gray
   switch (n.kind) {
      case NodeKind::User:
      case NodeKind::Party:
         return IM_COL32(80, 200, 120, 255);  // green
      case NodeKind::HeatedDSServer:
      case NodeKind::StandaloneServer:
      case NodeKind::HydraSample:
      case NodeKind::ProsSample:
         return IM_COL32(240, 160, 60, 255);  // orange
      case NodeKind::SCSession:
      case NodeKind::MMSession:
      case NodeKind::MMFlowSample:
      case NodeKind::DSSession:
         return IM_COL32(90, 160, 220, 255);  // blue
      case NodeKind::Unknown:
      default:
         return IM_COL32(140, 140, 140, 255); // gray
   }
}

void UnitsPanel::Draw()
{
   // Optional: tiny hint line, can remove anytime
   // ImGui::TextUnformatted("Units");
   // ImGui::Separator();

   // Render sections in a stable, intentional order:
   static const NodeKind order[] = {
       NodeKind::StandaloneServer,
       NodeKind::HeatedDSServer,
       NodeKind::DSSession,
       NodeKind::SCSession,
       NodeKind::MMFlowSample,
       NodeKind::ProsSample,
       NodeKind::HydraSample,
       NodeKind::User,
       NodeKind::Party,
       NodeKind::MMSession,
       NodeKind::Unknown,
   };

   for (NodeKind k : order)
      DrawKindSection(k);
}

bool UnitsPanel::DrawHeader(NodeKind kind, int count)
{
   // Header label "Users (2)" etc
   char header[64];
   snprintf(header, sizeof(header), "%s (%d)", KindLabel(kind), count);

   // Default open for all groups
   // can check (kind == NodeKind::MMFlowSample || kind == NodeKind::User || kind == NodeKind::HeatedDSServer)
   ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
   flags = ImGuiTreeNodeFlags_DefaultOpen;

   // Save style
   ImGui::PushStyleColor(ImGuiCol_Header, IM_COL32(0, 0, 0, 0));
   ImGui::PushStyleColor(ImGuiCol_HeaderHovered, IM_COL32(255, 255, 255, 20));
   ImGui::PushStyleColor(ImGuiCol_HeaderActive, IM_COL32(255, 255, 255, 30));

   //// Optional: tighter look
   //ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
   //ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

   bool open = ImGui::CollapsingHeader(header, flags);

   //// Restore style
   //ImGui::PopStyleVar(2);
   ImGui::PopStyleColor(3);

   return open;
}

void UnitsPanel::DrawKindSection(NodeKind kind)
{
   // Count nodes of this kind
   int count = 0;
   for (const GraphNode& n : model.nodes)
      if (n.kind == kind)
         ++count;

   // hide empty collections
   if (count == 0)
      return;

   // collapsible
   if (!DrawHeader(kind, count))
      return;

   // Build a list of pointers so we can sort by title without moving model data
   ImVector<const GraphNode*> items;
   items.reserve(count);
   for (const GraphNode& n : model.nodes)
      if (n.kind == kind)
         items.push_back(&n);

   std::sort(items.begin(), items.end(),
      [](const GraphNode* a, const GraphNode* b) {
         return a->title < b->title;
      });

   // Draw rows
   for (const GraphNode* n : items)
      DrawNodeRow(*n);
}

void UnitsPanel::DrawNodeRow(const GraphNode& n)
{
   // Row layout: [dot] [icon] [selectable text]
   // We'll use a tight row height so it feels "inventory-like".
   const float dotRadius = 4.0f;
   const ImU32 dotCol = StatusColorFor(n);
   float lineH = ImGui::GetTextLineHeightWithSpacing();

   // Dot -- disabled
   if (0) {
      ImVec2 p = ImGui::GetCursorScreenPos();
      ImDrawList* dl = ImGui::GetWindowDrawList();

      // Vertically center the dot inside current line height
      ImVec2 c = ImVec2(p.x + dotRadius + 1.0f, p.y + lineH * 0.5f);

      dl->AddCircleFilled(c, dotRadius, dotCol);
   }

   // Reserve space for the dot
   ImGui::Dummy(ImVec2(dotRadius * 2.0f + 8.0f, lineH));
   ImGui::SameLine();

   // Icon
   ImTextureID icon = tex.IconForKind(n.kind, 24.0f);
   if (icon != ImTextureID_Invalid) {
      // Use a small, consistent icon size for list readability
      const float iconSz = 16.0f;
      ImGui::Image(icon, ImVec2(iconSz, iconSz));
      ImGui::SameLine();
   }

   // Selectable label
   // Make ImGui ID stable even if titles repeat
   ImGui::PushID((int)n.id);

   //We don't want highlight to persist here, so pass false.
   const bool isSelected = false;// view.IsSelected(n.id);

   // Show title (and optionally subtitle dimmed)
   // Use Selectable for the whole row behavior.
   if (ImGui::Selectable(n.title.c_str(), isSelected)) {
      view.SelectNode(n.id);
   }

   // Optional: subtitle below (commented out; can enable later)
   // if (!n.subtitle.empty())
   // {
   //     ImGui::Indent(22.0f);
   //     ImGui::TextDisabled("%s", n.subtitle.c_str());
   //     ImGui::Unindent(22.0f);
   // }

   ImGui::PopID();
}
