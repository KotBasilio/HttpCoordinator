#include "inspector_panel.h"
#include <imgui.h>

#pragma message("inspector_panel.cpp REV: SC sessions v0.3")

static bool IsReorderable(NodeKind k)
{
   switch (k) {
      case NodeKind::User:
      case NodeKind::Party:
      case NodeKind::MMSession:
      case NodeKind::SCSession:
      case NodeKind::StandaloneServer:
      case NodeKind::HeatedDSServer:
         return true;
   }
   return false;
}

InspectorPanel::InspectorPanel(GraphViewState& view_, GraphModel& model_, Sample::Tex::TextureManager& tex_)
   : view(view_)
   , model(model_)
   , tex(tex_)
{
}

const char* InspectorPanel::ToString(NodeKind k)
{
   switch (k) {
      case NodeKind::Unknown:          return "Unknown";
      case NodeKind::DSSession:        return "DSSession";
      case NodeKind::HeatedDSServer:   return "HeatedDSServer";
      case NodeKind::SCSession:        return "SCSession";
      case NodeKind::MMFlowSample:     return "MMFlowSample";
      case NodeKind::User:             return "User";
      case NodeKind::Party:            return "Party";
      case NodeKind::MMSession:        return "MMSession";
      case NodeKind::StandaloneServer: return "StandaloneServer";
      case NodeKind::HydraSample:      return "HydraSample";
      default:                         return "<?>"; // future-proof
   }
}

void InspectorPanel::DrawEmpty()
{
   ImGui::TextUnformatted("No node selected.");
   ImGui::TextDisabled("Click a node in the Graph to inspect it.");
}

void InspectorPanel::DrawNotFound(NodeId id)
{
   ImGui::Text("Selected node id: %llu", id);
   ImGui::TextDisabled("(not found in model)");
}

void InspectorPanel::DrawNode(const GraphNode& n)
{
   // header: icon + title + subtitle
   ImTextureID icon = tex.IconForKind(n.kind);
   if (icon != ImTextureID_Invalid) {
      ImGui::Image(icon, ImVec2(32, 32));
      ImGui::SameLine();
   }
   ImGui::BeginGroup();
   ImGui::TextUnformatted(n.title.c_str());
   if (!n.subtitle.empty())
      ImGui::TextDisabled("%s", n.subtitle.c_str());
   ImGui::EndGroup();

   // basic minimal fields
   ImGui::Separator();
   const ImVec2 fieldsStart = ImGui::GetCursorPos(); // window-local
   ImGui::Text("Kind: %s", ToString(n.kind));
   ImGui::Text("Pos: (%.1f, %.1f)", n.pos.x, n.pos.y);
   ImGui::Text("Size: (%.1f, %.1f)", n.size.x, n.size.y);
   //ImGui::Text("Id: %llu", n.id);

   // move up/down
   DrawUpDownButtons(n, fieldsStart);

   // links
   DrawLinksIn(n.id);
   DrawLinksOut(n.id);

   // keys
   DrawNodeKeys(n);
}

// UpDownButtons are order controls (layout-only)
void InspectorPanel::DrawUpDownButtons(const GraphNode& n, const ImVec2& fieldsStart)
{
   // visible only for row-meaningful nodes
   if (!IsReorderable(n.kind)) return;

   // compute button width (use the wider label)
   const ImGuiStyle& st = ImGui::GetStyle();
   const float btnW = ImGui::CalcTextSize("down").x + st.FramePadding.x * 4.0f;
   const float lineH = ImGui::GetTextLineHeightWithSpacing();

   // right-align in the content region (window-local coordinates)
   float xRight = ImGui::GetContentRegionMax().x - btnW;

   // place starting around the "Pos" line so it sits in the marked area
   float yBtn = fieldsStart.y; // + lineH * 1.0f;

   // If the panel is too narrow (no empty space), fall back to old vertical placement below the fields
   const bool hasRoom = (xRight > fieldsStart.x + 40.0f);
   if (!hasRoom) {
      if (ImGui::Button("up"))   view.pendingUpDownDelta = -1;
      if (ImGui::Button("down")) view.pendingUpDownDelta = +1;
      return;
   }

   // up
   const ImVec2 fieldsEnd = ImGui::GetCursorPos();
   ImGui::SetCursorPos(ImVec2(xRight, yBtn));
   if (ImGui::Button("up"))   view.pendingUpDownDelta = -1;

   // down
   const ImVec2 lineDown = ImGui::GetCursorPos();
   ImGui::SetCursorPos(ImVec2(xRight, lineDown.y));
   if (ImGui::Button("down")) view.pendingUpDownDelta = +1;

   // restore cursor so layout continues normally under the fields
   ImGui::SetCursorPos(fieldsEnd);
}
void InspectorPanel::DrawNodeKeys(const GraphNode& n)
{
   ImGui::Separator();

   // we attempt to draw entityKey only if there's no USER_ID in kv
   if (!n.entityKey.empty()) {
      auto it = std::find_if(n.kv.begin(), n.kv.end(),
         [](const std::pair<std::string, std::string>& pair) { return pair.first == "USER_ID"; });
      if (it == n.kv.end()) {
         ImGui::Text("%s", n.entityKey.c_str());
      }
   }

   // kv is here => draw entire section
   if (!n.kv.empty()) {
      const float minHeight = 250.0f;
      float availY = ImGui::GetContentRegionAvail().y;
      float childHeight = std::max(minHeight, availY);
      ImGui::BeginChild("facts_kv", ImVec2(0, childHeight), true);

      if (ImGui::BeginTable("facts_tbl", 2,
         ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {

         ImGui::TableSetupColumn("propName", ImGuiTableColumnFlags_WidthFixed, 170.0f);
         ImGui::TableSetupColumn("propValue", ImGuiTableColumnFlags_WidthStretch);
         ImGui::TableHeadersRow();

         for (const auto& kv : n.kv) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(kv.first.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s", kv.second.c_str());
         }

         ImGui::EndTable();
      }

      ImGui::EndChild();
   }
}

void InspectorPanel::DrawClickableLink(const GraphLink& e, NodeId idDest)
{
   const GraphNode* dst = model.Find(idDest);
   if (!dst) {
      return;
   }

   // Build a label. simple + unique with PushID.
   const std::string label = (dst->title + "  (id " + std::to_string(dst->id) + ")");
   ImGui::PushID((int)(e.from ^ (e.to << 5)));

   // Make it clickable. We don't want highlight to persist here, so pass false.
   if (ImGui::Selectable(label.c_str(), false)) {
      view.selected.node = idDest;
   }

   ImGui::PopID();
}

void InspectorPanel::DrawLinksOut(NodeId selectedId)
{
   // Collect outgoing links
   int outCount = 0;
   for (const GraphLink& e : model.links)
      if (e.from == selectedId)
         ++outCount;

   // back off
   if (outCount == 0)
      return;

   ImGui::Separator();
   ImGui::Text("Outgoing: %d", outCount);

   // List them
   for (const GraphLink& e : model.links) {
      if (e.from != selectedId)
         continue;

      DrawClickableLink(e, e.to);
   }
}

void InspectorPanel::DrawLinksIn(NodeId selectedId)
{
   // Collect incoming links
   int inCount = 0;
   for (const GraphLink& e : model.links)
      if (e.to == selectedId)
         ++inCount;

   // back off
   if (inCount == 0)
      return;

   ImGui::Separator();
   ImGui::Text("Incoming: %d", inCount);

   // List them
   for (const GraphLink& e : model.links) {
      if (e.to != selectedId)
         continue;

      DrawClickableLink(e, e.from);
   }
}

void InspectorPanel::Draw()
{
   ImGui::TextUnformatted("Selected node details");
   ImGui::Separator();

   const NodeId sel = view.selected.node;
   if (sel == 0) {
      DrawEmpty();
      return;
   }

   const GraphNode* n = model.Find(sel);
   if (!n) {
      DrawNotFound(sel);
      return;
   }

   DrawNode(*n);
}
