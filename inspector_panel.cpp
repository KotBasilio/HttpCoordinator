#include "inspector_panel.h"
#include <imgui.h>
#include <algorithm>
#include <string>

#pragma message("inspector_panel.cpp REV: clickable kv v0.1")

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

static float ClampFloat(float lo, float v, float hi)
{
   if (hi < lo) hi = lo;
   return (v < lo) ? lo : (v > hi) ? hi : v;
}

static float CalcPropNameColumnWidth(const std::vector<std::pair<std::string, std::string>>& kv)
{
   const ImGuiStyle& style = ImGui::GetStyle();

   float w = ImGui::CalcTextSize("propName").x;

   for (const auto& row : kv) {
      w = std::max(w, ImGui::CalcTextSize(row.first.c_str()).x);
   }

   // Cell padding on both sides + small breathing room
   w += style.CellPadding.x * 2.0f + 5.0f;

   // Do not let propName eat the whole Inspector.
   const float avail = ImGui::GetContentRegionAvail().x;
   const float minW = 120.0f;
   const float maxW = std::max(minW, avail * 0.75f);

   return ClampFloat(minW, w, maxW);
}

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

static const GraphNode* FindUniqueNodeEchoForValue(const GraphModel& graph, const std::string& value)
{
   const GraphNode* found = nullptr;

   for (const GraphNode& node : graph.nodes) {
      if (!NodeEchoesValue(node, value)) {
         continue;
      }

      if (found != nullptr) {
         return nullptr;
      }

      found = &node;
   }

   return found;
}

void InspectorPanel::DrawMaybeClickableKvValue(const std::string& value)
{
   const GraphNode* target = FindUniqueNodeEchoForValue(model, value);
   if (!target) {
      ImGui::TextWrapped("%s", value.c_str());
      return;
   }

   ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
   ImGui::TextWrapped("%s", value.c_str());
   ImGui::PopStyleColor();

   if (ImGui::IsItemHovered()) {
      const std::string& jumpName = target->entityKey.empty() ? target->title : target->entityKey;
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      ImGui::SetTooltip("Jump to %s", jumpName.c_str());
   }

   if (ImGui::IsItemClicked()) {
      view.selected.node = target->id;
   }
}

void InspectorPanel::DrawNodeKeys(const GraphNode& n)
{
   ImGui::Separator();

   // entityKey always
   if (!n.entityKey.empty()) {
      ImGui::Text("%s", n.entityKey.c_str());
   }

   // kv is here => draw entire section
   if (!n.kv.empty()) {
      const float minHeight = 250.0f;
      float availY = ImGui::GetContentRegionAvail().y;
      float childHeight = std::max(minHeight, availY);
      ImGui::BeginChild("facts_kv", ImVec2(0, childHeight), true);

      ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
      if (ImGui::BeginTable("facts_tbl", 2, flags)) {
         float propNameWidth = CalcPropNameColumnWidth(n.kv);
         ImGui::TableSetupColumn("propName", ImGuiTableColumnFlags_WidthFixed, propNameWidth);
         ImGui::TableSetupColumn("propValue", ImGuiTableColumnFlags_WidthStretch);
         ImGui::TableHeadersRow();

         for (const auto& kv : n.kv) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(kv.first.c_str());
            ImGui::TableSetColumnIndex(1);
            DrawMaybeClickableKvValue(kv.second);
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
