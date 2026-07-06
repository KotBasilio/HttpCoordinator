#include "inspector_panel.h"
#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <map>
#include <sstream>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

static constexpr float kCopyIconPx = 16.0f;
static constexpr float kCopyButtonWidth = kCopyIconPx * 2.0f;
static constexpr const char* kHydraStub1Url = "https://www.google.com";
static constexpr const char* kHydraStub2Url = "https://stgadm.prismray.io/Title/RVSBBCE/Diagnostics/FactViewerPresetList";
static constexpr const char* kHydraFactsSessionBaseUrl = "https://stgadm.prismray.io/Title/RVSBBCE/Diagnostics/FactViewerPresetList/FactViewerSearchResult?query=";

// #define DBG_BADGES

// -- Helpers --
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
      default:
         return false;
   }
}

static bool IsHexDigit(char c)
{
   return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

static bool IsBase64ishChar(char c)
{
   const unsigned char uc = static_cast<unsigned char>(c);
   return std::isalnum(uc) || c == '+' || c == '/' || c == '=' || c == '_' || c == '-';
}

static bool IsNumericKvValue(std::string_view value)
{
   if (value.empty()) {
      return false;
   }

   std::string s(value);
   char* end = nullptr;
   (void)std::strtod(s.c_str(), &end);
   return end != s.c_str() && end && *end == '\0';
}

static bool IsBinaryLikeKvValue(std::string_view value)
{
   if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
      return std::all_of(value.begin() + 2, value.end(), IsHexDigit);
   }

   const bool allHex = !value.empty() && std::all_of(value.begin(), value.end(), IsHexDigit);
   if (allHex && value.size() >= 24) {
      return true;
   }

   const bool allBase64ish = !value.empty() && std::all_of(value.begin(), value.end(), IsBase64ishChar);
   return allBase64ish && value.size() >= 48;
}

static bool IsBooleanLikeKvValue(std::string_view value)
{
   // Convert the string to lowercase (C++17 has no direct std::string overload for tolower)
   std::string lowerValue(value);
   std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
      });

   // Check for common boolean-like values
   return lowerValue == "true" || lowerValue == "false" || lowerValue == "yes" || lowerValue == "no" || lowerValue == "1" || lowerValue == "0";
}

static const char* BadgeExplanation(NodeKind k)
{
   switch (k) {
      case NodeKind::Online:      return "Online";
      case NodeKind::Offline:     return "Offline";
      case NodeKind::PartyLeader: return "Owner/leader role";
      case NodeKind::LocalUser:   return "Local";
      default:                    return nullptr;
   }
}

// -- Implementation --
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
      case NodeKind::ProsSample:       return "ProsSample";
      case NodeKind::User:             return "User";
      case NodeKind::Party:            return "Party";
      case NodeKind::MMSession:        return "MMSession";
      case NodeKind::StandaloneServer: return "StandaloneServer";
      case NodeKind::HydraSample:      return "HydraSample";
      case NodeKind::ConnectorCross:   return "ConnectorCross";
      case NodeKind::ConnectorEnd:     return "ConnectorEnd";
      case NodeKind::ConnectorStart:   return "ConnectorStart";
      case NodeKind::LocalSample:      return "LocalSample";
      case NodeKind::LocalServer:      return "LocalServer";
      case NodeKind::LocalUser:        return "LocalUser";
      case NodeKind::Offline:          return "Offline";
      case NodeKind::Online:           return "Online";
      case NodeKind::PartyLeader:      return "PartyLeader";
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
   ExplainIcon(n);

   // basic minimal fields
   ImGui::Separator();
   const ImVec2 fieldsStart = ImGui::GetCursorPos(); // window-local
   ImGui::Text("Kind: %s", ToString(n.kind));
   ImGui::Text("Pos: (%.1f, %.1f)", n.pos.x, n.pos.y);
   if (view.selected.lodPx > 0) {
      ImGui::Text("Size:(%3.0f, %3.0f)", view.selected.drawW, view.selected.drawH);
      ImGui::SameLine();
      ImGui::Text("LOD: %d px", view.selected.lodPx);
   } else {
      ImGui::Text("Size: (%.1f, %.1f)", n.size.x, n.size.y);
   }

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

void InspectorPanel::ExplainIcon(const GraphNode& n)
{
   ImTextureID icon = tex.IconForKind(n.kind);
   if (icon != ImTextureID_Invalid) {
      ImGui::Image(icon, ImVec2(32, 32));
      ImGui::SameLine();
   }
   ImGui::BeginGroup();
   ImGui::TextUnformatted(n.title.c_str());

   if (!n.subtitle.empty()) {
      //if (!n.badges.empty()) {
      //   ImGui::SameLine();
      //}
      ImGui::TextDisabled("%s", n.subtitle.c_str());
   }
   ImGui::EndGroup();
   DrawBadges(n);
}

void InspectorPanel::DrawBadges(const GraphNode& n)
{
   if (n.badges.empty())
      return;

   //ImGui::Separator();
   //ImGui::TextUnformatted("Badges:");

   for (NodeKind badge : n.badges) {
      const char* explanation = BadgeExplanation(badge);
      if (!explanation)
         continue;

      ImTextureID icon = tex.IconForKind(badge, 20.0f);
      if (icon != ImTextureID_Invalid) {
         ImGui::Image(icon, ImVec2(16.0f, 16.0f));
         ImGui::SameLine();
      }

      ImGui::Text("- %s", explanation);

      #ifdef DBG_BADGES
         if (badge == NodeKind::PartyLeader) {
            ImGui::SameLine();
            ImGui::Text("%3.1f -> %3.1f", view.selected.badgeDbg1, view.selected.badgeDbg2);
         }
      #endif // DBG_BADGES
   }
}

static float ClampFloat(float lo, float v, float hi)
{
   if (hi < lo) hi = lo;
   return (v < lo) ? lo : (v > hi) ? hi : v;
}

static std::string FindKvValue(std::string_view key, const std::vector<std::pair<std::string, std::string>>& kv)
{
   const auto it = std::find_if(kv.begin(), kv.end(), [key](const auto& row) {
      return row.first == key;
   });
   return (it == kv.end()) ? std::string{} : it->second;
}

static bool OpenUrlInBrowser(const std::string& url)
{
#ifdef _WIN32
   const HINSTANCE result = ::ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
   return reinterpret_cast<intptr_t>(result) > 32;
#else
   (void)url;
   return false;
#endif
}

static std::string UrlEncode(std::string_view value)
{
   static constexpr char hex[] = "0123456789ABCDEF";

   std::string encoded;
   encoded.reserve(value.size() * 3);
   for (unsigned char ch : value) {
      if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
         encoded.push_back(static_cast<char>(ch));
         continue;
      }

      encoded.push_back('%');
      encoded.push_back(hex[(ch >> 4) & 0x0F]);
      encoded.push_back(hex[ch & 0x0F]);
   }
   return encoded;
}

static bool FormatUtcTimestamp(std::chrono::system_clock::time_point tp, std::string& out)
{
   using namespace std::chrono;

   const auto secs = time_point_cast<seconds>(tp);
   const auto millis = duration_cast<milliseconds>(tp - secs).count();
   std::time_t tt = system_clock::to_time_t(secs);

   std::tm tmUtc{};
#ifdef _WIN32
   if (::gmtime_s(&tmUtc, &tt) != 0)
      return false;
#else
   if (::gmtime_r(&tt, &tmUtc) == nullptr)
      return false;
#endif

   char buffer[32];
   const size_t written = std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tmUtc);
   if (written == 0)
      return false;

   char millisBuf[8];
   std::snprintf(millisBuf, sizeof(millisBuf), ".%03lldZ", static_cast<long long>(millis));
   out.assign(buffer, written);
   out += millisBuf;
   return true;
}

static std::string BuildHydraFactsSessionUrl(const std::string& hydraKernelSessionId)
{
   using namespace std::chrono;

   const system_clock::time_point now = system_clock::now();
   std::string startUtc;
   std::string endUtc;
   if (!FormatUtcTimestamp(now - hours(6), startUtc) || !FormatUtcTimestamp(now + hours(6), endUtc)) {
      return std::string{};
   }

   std::ostringstream json;
   json << "{\"session\":\"" << hydraKernelSessionId
      << "\",\"start\":\"" << startUtc
      << "\",\"end\":\"" << endUtc
      << "\",\"useStrictDateInclusion\":false,\"contexts\":[]}";

   return std::string(kHydraFactsSessionBaseUrl) + UrlEncode(json.str());
}

static float CalcPropNameColumnWidth(const std::vector<std::pair<std::string, std::string>>& kv)
{
   const ImGuiStyle& style = ImGui::GetStyle();

   float w = ImGui::CalcTextSize("propName").x;

   for (const auto& row : kv) {
      w = max(w, ImGui::CalcTextSize(row.first.c_str()).x);
   }

   // Cell padding on both sides + small breathing room
   w += style.CellPadding.x * 2.0f + 5.0f;

   // Do not let propName eat the whole Inspector.
   const float avail = ImGui::GetContentRegionAvail().x;
   const float minW = 120.0f;
   const float maxW = max(minW, avail * 0.75f);

   return ClampFloat(minW, w, maxW);
}

static std::string BuildKvCopyText(std::string_view key, std::string_view value)
{
   if (IsNumericKvValue(value) || IsBooleanLikeKvValue(value)) {
      std::string out(key);
      out += "=";
      out += value;
      return out;
   }

   return std::string(value);
}

struct GroupedMemberKv
{
   int memberIndex = 0;
   std::vector<std::pair<std::string, std::string>> fields;
};

static bool ParseMemberKvKey(std::string_view key, int& memberIndex, std::string& fieldName)
{
   static constexpr std::string_view prefix = "MEMBER_";
   if (key.substr(0, prefix.size()) != prefix)
      return false;

   const size_t idxStart = prefix.size();
   const size_t idxEnd = key.find('_', idxStart);
   if (idxEnd == std::string_view::npos || idxEnd == idxStart)
      return false;

   for (size_t i = idxStart; i < idxEnd; ++i) {
      if (!std::isdigit(static_cast<unsigned char>(key[i])))
         return false;
   }

   memberIndex = std::atoi(std::string(key.substr(idxStart, idxEnd - idxStart)).c_str());
   if (memberIndex < 0 || idxEnd + 1 >= key.size())
      return false;

   fieldName = std::string(key.substr(idxEnd + 1));
   return !fieldName.empty();
}

void InspectorPanel::DrawMaybeClickableKvValue(const std::string& value)
{
   const GraphNode* target = model.FindUniqueNodeEchoForValue(value);
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
      view.SelectNode(target->id);
   }
}

void InspectorPanel::DrawKvCopyButton(int rowIndex, const std::string& key, const std::string& value)
{
   const std::string copyText = BuildKvCopyText(key, value);
   const float buttonHeight = ImGui::GetTextLineHeightWithSpacing();
   const ImVec2 buttonSize(kCopyButtonWidth, buttonHeight);

   ImGui::PushID(rowIndex);
   if (ImGui::InvisibleButton("copy", buttonSize)) {
      ImGui::SetClipboardText(copyText.c_str());
   }

   const ImVec2 buttonMin = ImGui::GetItemRectMin();
   const ImVec2 buttonMax = ImGui::GetItemRectMax();
   ImDrawList* dl = ImGui::GetWindowDrawList();
   if (ImGui::IsItemHovered()) {
      dl->AddRectFilled(buttonMin, buttonMax, ImGui::GetColorU32(ImGuiCol_ButtonHovered), ImGui::GetStyle().FrameRounding);
   }

   ImTextureID copyIcon = tex.Access(AssetID::IC_COPY_16PX);
   if (copyIcon != ImTextureID_Invalid) {
      const ImVec2 iconPos(
         buttonMin.x + (buttonSize.x - kCopyIconPx) * 0.5f,
         buttonMin.y + (buttonSize.y - kCopyIconPx) * 0.5f
      );
      dl->AddImage(copyIcon, iconPos, ImVec2(iconPos.x + kCopyIconPx, iconPos.y + kCopyIconPx));
   }
   ImGui::PopID();

   if (ImGui::IsItemHovered()) {
      auto textTooltip = "Copy:" + copyText;
      ImGui::SetTooltip("%s", textTooltip.c_str());
   }
}

void InspectorPanel::DrawKeyValTable(const GraphNode& n)
{
   std::vector<std::pair<std::string, std::string>> flatRows;
   std::vector<GroupedMemberKv> groupedMembers;

   if (n.kind == NodeKind::Party || n.kind == NodeKind::MMSession) {
      std::map<int, std::vector<std::pair<std::string, std::string>>> grouped;
      for (const auto& kv : n.kv) {
         int memberIndex = 0;
         std::string fieldName;
         if (ParseMemberKvKey(kv.first, memberIndex, fieldName)) {
            grouped[memberIndex].push_back({ fieldName, kv.second });
         } else {
            flatRows.push_back(kv);
         }
      }

      for (auto& [memberIndex, fields] : grouped) {
         groupedMembers.push_back({ memberIndex, std::move(fields) });
      }
   } else {
      flatRows = n.kv;
   }

   const float minHeight = 250.0f;
   float availY = ImGui::GetContentRegionAvail().y;
   float childHeight = max(minHeight, availY);
   ImGui::BeginChild("facts_kv", ImVec2(0, childHeight), true);

   ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
   if (ImGui::BeginTable("facts_tbl", 3, flags)) {
      auto drawFlatKvRows = [&](const std::vector<std::pair<std::string, std::string>>& rows, int& rowIndex) {
         for (const auto& kv : rows) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(kv.first.c_str());
            ImGui::TableSetColumnIndex(1);
            DrawMaybeClickableKvValue(kv.second);
            ImGui::TableSetColumnIndex(2);
            DrawKvCopyButton(rowIndex, kv.first, kv.second);
            ++rowIndex;
         }
      };

      auto drawGroupedMemberKvRows = [&](const std::vector<GroupedMemberKv>& members, int& rowIndex) {
         if (members.empty())
            return;

         ImGui::TableNextRow();
         ImGui::TableSetColumnIndex(0);
         const std::string label = "MEMBERS[size=" + std::to_string(members.size()) + "]";
         const bool membersOpen = ImGui::TreeNodeEx("members_array",
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanFullWidth, "%s", label.c_str());
         ImGui::TableSetColumnIndex(1);
         ImGui::TextUnformatted("");
         ImGui::TableSetColumnIndex(2);
         ImGui::Dummy(ImVec2(0.0f, 0.0f));

         if (membersOpen) {
            for (const auto& member : members) {
               ImGui::TableNextRow();
               ImGui::TableSetColumnIndex(0);
               std::string memberLabel = "member[" + std::to_string(member.memberIndex) + "]";
               const std::string memberId = "member_" + std::to_string(member.memberIndex);
               const bool memberOpen = ImGui::TreeNodeEx(memberId.c_str(),
                  ImGuiTreeNodeFlags_SpanFullWidth, "%s", memberLabel.c_str());
               ImGui::TableSetColumnIndex(1);
               ImGui::TextUnformatted("");
               ImGui::TableSetColumnIndex(2);
               ImGui::Dummy(ImVec2(0.0f, 0.0f));

               if (memberOpen) {
                  for (const auto& field : member.fields) {
                     ImGui::TableNextRow();
                     ImGui::TableSetColumnIndex(0);
                     ImGui::Indent();
                     ImGui::TextUnformatted(field.first.c_str());
                     ImGui::Unindent();
                     ImGui::TableSetColumnIndex(1);
                     DrawMaybeClickableKvValue(field.second);
                     ImGui::TableSetColumnIndex(2);
                     DrawKvCopyButton(rowIndex, field.first, field.second);
                     ++rowIndex;
                  }
                  ImGui::TreePop();
               }
            }
            ImGui::TreePop();
         }
      };

      float propNameWidth = CalcPropNameColumnWidth(flatRows.empty() ? n.kv : flatRows);
      ImGui::TableSetupColumn("propName", ImGuiTableColumnFlags_WidthFixed, propNameWidth);
      ImGui::TableSetupColumn("propValue", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, kCopyButtonWidth);
      ImGui::TableHeadersRow();

      int rowIndex = 0;
      drawFlatKvRows(flatRows, rowIndex);
      drawGroupedMemberKvRows(groupedMembers, rowIndex);

      ImGui::EndTable();
   }

   ImGui::EndChild();
}

void InspectorPanel::DrawHydraActions(const GraphNode& n)
{
   const std::string hydraKernelSessionId = FindKvValue("HYDRA_KERNEL_SESSION_ID", n.kv);
   const bool hasHydraKernelSessionId = !hydraKernelSessionId.empty();

   if (ImGui::Button("Stub1")) {
      OpenUrlInBrowser(kHydraStub1Url);
   }

   ImGui::SameLine();
   if (ImGui::Button("Stub2")) {
      OpenUrlInBrowser(kHydraStub2Url);
   }

   ImGui::SameLine();
   if (!hasHydraKernelSessionId) {
      ImGui::BeginDisabled();
   }

   if (ImGui::Button("View Facts Session") && hasHydraKernelSessionId) {
      const std::string url = BuildHydraFactsSessionUrl(hydraKernelSessionId);
      if (!url.empty()) {
         OpenUrlInBrowser(url);
      }
   }

   if (!hasHydraKernelSessionId) {
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered()) {
         ImGui::SetTooltip("HYDRA_KERNEL_SESSION_ID is missing");
      }
   }
}

void InspectorPanel::DrawNodeKeys(const GraphNode& n)
{
   ImGui::Separator();

   // entityKey always
   if (!n.entityKey.empty()) {
      ImGui::Text("%s", n.entityKey.c_str());
   }

   if (n.kind == NodeKind::HydraSample) {
      DrawHydraActions(n);
      ImGui::Separator();
   }

   // kv is here => draw entire section
   if (!n.kv.empty()) {
      DrawKeyValTable(n);
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
      view.SelectNode(idDest);
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
