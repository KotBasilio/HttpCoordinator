#include "main_window.h"
#include "imgui_internal.h" // DockBuilder* live here
#include "ui/models/main_model.h"
#include "ui/utils/texture_manager.h"

namespace Sample::UI::Views {

static void DrawUnitsClearButton(Sample::Tex::TextureManager& tex)
{
   constexpr float iconSize = 48.0f;

   const ImVec2 windowPos = ImGui::GetWindowPos();
   const ImVec2 windowSize = ImGui::GetWindowSize();
   const ImGuiStyle& style = ImGui::GetStyle();
   const float scrollbarInset = ImGui::GetScrollMaxY() > 0.0f ? style.ScrollbarSize : 0.0f;
   const ImVec2 buttonPos(
      windowPos.x + windowSize.x - style.WindowPadding.x - scrollbarInset - iconSize,
      windowPos.y + windowSize.y - style.WindowPadding.y - iconSize
   );

   ImGui::SetCursorScreenPos(buttonPos);
   ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
   ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 255, 255, 25));
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 255, 255, 45));

   (void)ImGui::ImageButton("##units_clear",
      tex.Access(AssetID::IC_CLEAR_GREY_48PX),
      ImVec2(iconSize, iconSize));

   ImGui::PopStyleColor(3);
   ImGui::PopStyleVar();

   if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
      ImGui::SetTooltip("Clear");
   }
}

MainWindow::MainWindow(Sample::UI::Models::MainModel* model, Sample::UI::Controllers::MainControllerInterface* controller)
   : mainModel(model)
   , mainController(controller)
   , gGraphPanel(model->view, model->graph, gTex)
   , gInspectorPanel(model->view, model->graph, gTex)
   , gUnitsPanel(model->view, model->graph, gTex)
   , gLogsPanel(model->logs)
{
   //gTex.Prefetch(".\\Assets\\AssetList.txt");
   gTex.Prefetch();
}

void MainWindow::Show()
{
   switch (mainModel->appState) {
      case Models::State::DEFAULT:
         {
            ShowDefaultWindow();
            break;
         }
      case Models::State::USER_LOGGING:
         {
            break;
         }
      case Models::State::SERVER_STARTING:
         {
            break;
         }
      case Models::State::USER_LOGGED:
         {
            break;
         }
      case Models::State::SERVER_STARTED:
         {
            break;
         }
      default:
         {
         }
   }
}

static void BuildDefaultDockLayout(ImGuiID dockspace_id)
{
   // If already built, don't rebuild (preserve user layout from imgui.ini)
   if (ImGui::DockBuilderGetNode(dockspace_id) != nullptr)
      return;

   ImGuiViewport* vp = ImGui::GetMainViewport();

   ImGui::DockBuilderRemoveNode(dockspace_id);
   ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
   ImGui::DockBuilderSetNodeSize(dockspace_id, vp->WorkSize);

   // Split: left | center | right
   ImGuiID dock_main   = dockspace_id;
   ImGuiID dock_left   = 0;
   ImGuiID dock_right  = 0;
   ImGuiID dock_center = dock_main; // dock_center is now the top region (Graph)

   // Left 20%
   dock_left = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Left, 0.20f, nullptr, &dock_center);

   // Right 25% (of remaining)
   dock_right = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right, 0.25f, nullptr, &dock_center);

   // Center split: top (Graph) and bottom (Logs)
   ImGuiID dock_logs = 0;
   dock_logs = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Down, 0.30f, nullptr, &dock_center);

   // Dock windows (names must match ImGui::Begin titles exactly)
   ImGui::DockBuilderDockWindow("Units",     dock_left);
   ImGui::DockBuilderDockWindow("Inspector", dock_right);
   ImGui::DockBuilderDockWindow("Graph",     dock_center);
   ImGui::DockBuilderDockWindow("Logs",      dock_logs);

   ImGui::DockBuilderFinish(dockspace_id);
}

void MainWindow::RefreshDockingSetup()
{
   // Fullscreen host window over the main viewport
   ImGuiViewport* vp = ImGui::GetMainViewport();

   ImGui::SetNextWindowPos(vp->WorkPos);
   ImGui::SetNextWindowSize(vp->WorkSize);
   ImGui::SetNextWindowViewport(vp->ID);

   ImGuiWindowFlags host_flags =
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_MenuBar;

   ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

   ImGui::Begin("##DockHost", nullptr, host_flags);

   ImGui::PopStyleVar(3);

   // DockSpace
   const ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
   ImGuiDockNodeFlags dock_flags = ImGuiDockNodeFlags_None;

   // Prevent docking into the central node (common for "canvas" apps)
   dock_flags |= ImGuiDockNodeFlags_NoDockingInCentralNode;

   ImGui::DockSpace(dockspace_id, ImVec2(0, 0), dock_flags);

   // Build default layout (only if no node exists yet)
   BuildDefaultDockLayout(dockspace_id);

   // docked menu
   if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("Start")) {
         ImGui::MenuItem("Matchmaking", nullptr, &mainModel->matchmakingCheck);
         ImGui::MenuItem("QR flow", nullptr, &mainModel->qrFlowCheck);
         ImGui::MenuItem("Game Configuration", nullptr, &mainModel->gameConfigCheck);

         ImGui::Separator();

         ImGui::MenuItem("Hydra OSS", nullptr, &mainModel->hydraOssCheck);
         ImGui::MenuItem("Lyra", nullptr, &mainModel->lyraCheck);
         ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
   }

   ImGui::End();
}

void Sample::UI::Views::MainWindow::RenderAllPanes()
{
   // Dev switch
   #ifdef MINIMAL_PANES
      ImGui::Begin("Units");     ImGui::Text("Units / navigation go here"); ImGui::End();
      ImGui::Begin("Graph");     ImGui::Text("Flow visualization canvas");  ImGui::End();
      ImGui::Begin("Inspector"); ImGui::Text("Selected node details");      ImGui::End();
      ImGui::Begin("Logs");      ImGui::Text("Runtime logs");               ImGui::End();
      return;
   #endif // MINIMAL_PANES

   // Draw four panels normally (they'll dock automatically)
   // Left: Units
   if (ImGui::Begin("Units")) {
      gUnitsPanel.Draw();
      DrawUnitsClearButton(gTex);
      ImGui::End();
   }

   // Right: Inspector
   if (ImGui::Begin("Inspector")) {
      gInspectorPanel.Draw();
      ImGui::End();
   }

   // Center top: Graph
   if (ImGui::Begin("Graph")) {
      gGraphPanel.Draw();
      ImGui::End();
   }

   // Center bottom: Logs
   if (ImGui::Begin("Logs")) {
      gLogsPanel.Draw();
      ImGui::End();
   }
}

void MainWindow::ConsiderLaunchingSample()
{
   //if (mainModel->matchmakingCheck)
   //    mainController->ShowMatchmakingWindow();

   //if (mainModel->qrFlowCheck)
   //    mainController->ShowQRFlowWindow();

   //if (mainModel->gameConfigCheck)
   //    mainController->ShowGameConfigWindow();

   //if (mainModel->hydraOssCheck)
   //    mainController->ShowHydraOSSWindow();

   //if (mainModel->lyraCheck)
   //    mainController->ShowLyraWindow();
}

void MainWindow::ShowDefaultWindow()
{
   RefreshDockingSetup();
   RenderAllPanes();
   ConsiderLaunchingSample();
}

} // namespace Sample::UI::Views
