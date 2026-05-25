#include "graph_panel.h"

#include <algorithm>

namespace {

struct MockAssetPreview {
   AssetID asset = ASSET_COUNT;
};

// Visual shelf for IDs listed under Hanging Texture IDs in md-files/obsolete_tex.md.
static const MockAssetPreview kHangingTexturePreview[] = {
   { AssetID::BGPATTERN },
   { AssetID::SCALECONTROL },
   { AssetID::ARROWLEFT },
   { AssetID::ARROWLEFT_OVERLAYSHADOW },
   { AssetID::ARROWRIGHT },
   { AssetID::ARROWRIGHT_OVERLAYSHADOW },
   { AssetID::CLEARLOG },
   { AssetID::DRAGLISTHOVER },
   { AssetID::FILTER },
   { AssetID::CONNECTORCROSS_2PX },
   { AssetID::CONNECTORCROSS_4PX },
   { AssetID::CONNECTORCROSS_5PX },
   { AssetID::CONNECTORCROSS_8PX },
   { AssetID::CONNECTOREND_24PX },
   { AssetID::CONNECTOREND_2PX },
   { AssetID::CONNECTOREND_4PX },
   { AssetID::CONNECTOREND_5PX },
   { AssetID::CONNECTOREND_8PX },
   { AssetID::CONNECTORSTART_24PX },
   { AssetID::CONNECTORSTART_2PX },
   { AssetID::CONNECTORSTART_4PX },
   { AssetID::CONNECTORSTART_5PX },
   { AssetID::CONNECTORSTART_8PX },
   { AssetID::IC_ADD },
   { AssetID::IC_ARROW_BOTTOM },
   { AssetID::IC_ARROW_RATING_DECREASE },
   { AssetID::IC_ARROW_RATING_INCREASE },
   { AssetID::IC_ARROW_TOP },
   { AssetID::IC_ERROR_16_PX },
   { AssetID::IC_HISTORY_16PX },
   { AssetID::IC_INFORMATION_16_PX },
   { AssetID::IC_LOGS_16_PX },
   { AssetID::IC_RUN },
   { AssetID::IC_WARNING_16_PX },
   { AssetID::LOCALSAMPLE_10PX },
   { AssetID::LOCALSAMPLE_16PX },
   { AssetID::LOCALSAMPLE_24PX },
   { AssetID::LOCALSAMPLE_36PX },
   { AssetID::LOCALSAMPLE_7PX },
   { AssetID::LOCALSERVER_10PX },
   { AssetID::LOCALSERVER_16PX },
   { AssetID::LOCALSERVER_24PX },
   { AssetID::LOCALSERVER_36PX },
   { AssetID::LOCALSERVER_7PX },
   { AssetID::LOCALUSER_10PX },
   { AssetID::LOCALUSER_16PX },
   { AssetID::LOCALUSER_24PX },
   { AssetID::LOCALUSER_36PX },
   { AssetID::LOCALUSER_7PX },
   { AssetID::OFFLINE_11PX },
   { AssetID::OFFLINE_17PX },
   { AssetID::OFFLINE_26PX },
   { AssetID::OFFLINE_5PX },
   { AssetID::OFFLINE_7PX },
   { AssetID::ONLINE_11PX },
   { AssetID::ONLINE_17PX },
   { AssetID::ONLINE_26PX },
   { AssetID::ONLINE_5PX },
   { AssetID::ONLINE_7PX },
   { AssetID::PARTYLEADER_13PX },
   { AssetID::PARTYLEADER_20PX },
   { AssetID::PARTYLEADER_30PX },
   { AssetID::PARTYLEADER_46PX },
   { AssetID::PARTYLEADER_8PX },
};

} // namespace

void GraphPanel::EnsureModelPresence()
{
   if (!model.nodes.empty()) {
      return;
   }

   // Demo nodes if model empty
   model.links.clear();

   // Layout: 3 columns-ish, left->right flow
   const Vec2f s32{ 32, 32 };

   // IDs
   const NodeId N_USER0 = 1;
   const NodeId N_USER1 = 11;
   const NodeId N_USER2 = 13;
   const NodeId N_PARTY = 2;
   const NodeId N_SCSESS = 3;
   const NodeId N_DSSESS = 4;
   const NodeId N_MMFLOW = 5;
   const NodeId N_MMSESS = 6;
   const NodeId N_HEATED = 7;
   const NodeId N_STANDALONE = 8;
   const NodeId N_HYDRA = 9;
   const NodeId N_UNKNOWN = 10;
   const NodeId N_INFO2 = 12;
   const NodeId N_PROS = 14;

   // Column X positions
   const float x0 = 80.0f;
   const float x1 = 260.0f;
   const float x2 = 440.0f;
   const float x3 = 620.0f;

   // Row Y positions
   const float yS = 25.0f;
   const float yZ = yS + 20.0f;
   const float y0 = yS + 80.0f;
   const float yH = yS + 160.0f;
   const float y1 = yS + 240.0f;
   const float y2 = yS + 320.0f;
   const float y3 = yS + 380.0f;

   // Nodes: icon + title
   model.nodes.push_back(GraphNode{ N_USER0,      NodeKind::User,             "User0",           "BroUser50",      Vec2f{x0, yZ}, s32 });
   model.nodes.push_back(GraphNode{ N_USER1,      NodeKind::User,             "User1",           "ServUser50",     Vec2f{x0, y0}, s32 });
   model.nodes.push_back(GraphNode{ N_USER2,      NodeKind::User,             "User2",           "titleU",         Vec2f{x0, yH},  s32 });

   model.nodes.push_back(GraphNode{ N_PARTY,      NodeKind::Party,            "Party",           "Party#12",       Vec2f{x0, y1}, s32 });
   model.nodes.push_back(GraphNode{ N_SCSESS,     NodeKind::SCSession,        "SC Session",      "SCSession#3",    Vec2f{x0, y2}, s32 });
   model.nodes.push_back(GraphNode{ N_INFO2,      NodeKind::Unknown,          "Info2",           "titleH",         Vec2f{x3, y3},   s32 });

   model.nodes.push_back(GraphNode{ N_DSSESS,     NodeKind::DSSession,        "DS Session",      "DSSession#7",    Vec2f{x1, y0}, s32 });
   model.nodes.push_back(GraphNode{ N_MMFLOW,     NodeKind::MMFlowSample,     "MM Flow",         "FlowSample",     Vec2f{x1, y1}, s32 });
   model.nodes.push_back(GraphNode{ N_MMSESS,     NodeKind::MMSession,        "MM Session",      "MMSession#2",    Vec2f{x1, y2}, s32 });
   model.nodes.push_back(GraphNode{ N_PROS,       NodeKind::ProsSample,       "Pros",            "ProsSample",     Vec2f{x2, y1}, s32 });

   model.nodes.push_back(GraphNode{ N_HEATED,     NodeKind::HeatedDSServer,   "Heated DS",       "Server A",       Vec2f{x2, yH}, s32 });
   model.nodes.push_back(GraphNode{ N_STANDALONE, NodeKind::StandaloneServer, "Standalone",      "Server B",       Vec2f{x2, y2}, s32 });

   model.nodes.push_back(GraphNode{ N_HYDRA,      NodeKind::HydraSample,      "Hydra",           "OSS",            Vec2f{x3, y1}, s32 });
   model.nodes.push_back(GraphNode{ N_UNKNOWN,    NodeKind::Unknown,          "Info1",           "?",              Vec2f{x2, yZ}, s32 });

   // Links, generally left->right
   model.links.push_back(GraphLink{ N_USER0,  N_DSSESS }); // 1
   model.links.push_back(GraphLink{ N_PARTY,  N_MMFLOW }); // 2
   model.links.push_back(GraphLink{ N_SCSESS, N_MMSESS }); // 3

   model.links.push_back(GraphLink{ N_DSSESS, N_HEATED }); // 4
   model.links.push_back(GraphLink{ N_MMFLOW, N_HEATED }); // 5
   model.links.push_back(GraphLink{ N_MMSESS, N_STANDALONE }); // 6

   model.links.push_back(GraphLink{ N_HEATED, N_HYDRA }); // 7
   model.links.push_back(GraphLink{ N_STANDALONE, N_HYDRA }); // 8

   model.links.push_back(GraphLink{ N_DSSESS, N_UNKNOWN }); // 9
   model.links.push_back(GraphLink{ N_UNKNOWN, N_HYDRA }); // 10

   model.links.push_back(GraphLink{ N_USER1, N_DSSESS });
   model.links.push_back(GraphLink{ N_USER2, N_DSSESS });
   model.links.push_back(GraphLink{ N_STANDALONE, N_INFO2 });
   model.links.push_back(GraphLink{ N_PROS, N_HYDRA });

   model.RebuildIndex();
}

void GraphPanel::RenderHangingAssetPreview(ImDrawList* dl)
{
   if (model.projectionGeneration > 0) {
      return;
   }

   const float icon = 20.0f;
   const float cell = 28.0f;
   const float pad = 8.0f;
   const int count = (int)(sizeof(kHangingTexturePreview) / sizeof(kHangingTexturePreview[0]));
   const float availableW = std::max(80.0f, canvas_size.x - pad * 2.0f - 50.f);
   const int columns = std::max(1, (int)(availableW / cell));
   const int rows = (count + columns - 1) / columns;
   const ImVec2 titleSize = ImGui::CalcTextSize("Hanging textures");
   const ImVec2 boxMin(canvas_pos.x + pad, canvas_max.y - pad - titleSize.y - 6.0f - rows * cell);
   const ImVec2 boxMax(canvas_max.x - pad - 45.f, canvas_max.y - pad);

   dl->AddRectFilled(boxMin, boxMax, IM_COL32(18, 18, 18, 220), 4.0f);
   dl->AddRect(boxMin, boxMax, IM_COL32(80, 80, 80, 180), 4.0f);
   dl->AddText(ImVec2(boxMin.x + 6.0f, boxMin.y + 4.0f), IM_COL32(190, 205, 220, 255), "Hanging textures");

   const float startX = boxMin.x + 6.0f;
   const float startY = boxMin.y + titleSize.y + 8.0f;
   for (int i = 0; i < count; ++i) {
      const int col = i % columns;
      const int row = i / columns;
      const ImVec2 p(startX + (float)col * cell, startY + (float)row * cell);
      ImTextureID texId = tex.Access(kHangingTexturePreview[i].asset);
      if (texId == ImTextureID_Invalid) {
         dl->AddRect(p, ImVec2(p.x + icon, p.y + icon), IM_COL32(180, 60, 60, 180), 2.0f);
         continue;
      }

      dl->AddImage(texId, p, ImVec2(p.x + icon, p.y + icon));
   }
}
