#include "ui/controllers/start_server_controller.h"
#include "ui/controllers/coordinator_http_server.h"

#pragma message("start_server_controller.cpp REV: SC sessions v0.1")

namespace Sample::UI::Controllers {

StartServerController::StartServerController(Models::MainModel* model, Sample::Connector::BackendConnectorInterface* connector)
   : backendConnector(connector)
   , server(std::make_unique<CoordinatorHttpServer>(model))
   , mainModel(model)
{
}

StartServerController::~StartServerController() = default; 

bool StartServerController::StartServer(const Models::ServerType serverType)
{
   server->Start();
   return true;
}

void StartServerController::StopServer()
{
   server->Stop();
}


void StartServerController::SetStartServerServiceState(const UI::Models::ServiceState& serviceState)
{
   startServerModel.serviceState = serviceState;
}


UI::Models::ServiceState StartServerController::GetServiceState() const
{
   return startServerModel.serviceState;
}

void StartServerController::TickIngestion()
{
   if (!mainModel) return;

   // for all packets
   SdkPacket u;
   bool changed = false;
   while (server->TryPop(u)) {
      if (!u.payload.is_object()) {
         mainModel->logs.Warn("TickIngestion: payload is not a JSON object");
         continue;
      }

      SwitchGraphToIngestion();
      changed |= ApplyAllReducers(u);
   }

   // may reorder nodes in layout
   changed |= ApplyReorderCommands();

   // may repaint the graph from state
   if (changed) {
      ProjectToGraph();
   }
}

void StartServerController::SwitchGraphToIngestion()
{
   // First packet: switch graph to ingestion-driven (wipe any demo seed created elsewhere)
   if (!mainModel->graphUsesIngestion) {
      mainModel->graphUsesIngestion = true;
      auto& graph = mainModel->graph;
      graph.nodes.clear();
      graph.links.clear();
      graph.indexById.clear();
      mainModel->logs.Info("first packet -> switching graph to ingestion-driven");
   }
}

bool StartServerController::ApplyAllReducers(SdkPacket& u)
{
   // a few switch-es to separate different logical areas
   // -- user
   switch (u.reqNameId) {
      case PROS_GLOBAL_API_AUTH_SIGNINHYDRARESPONSE:
      case HYDRA_API_USER_CONNECTRESPONSE:              return HandleSignIn(u);

      case PROS_GLOBAL_API_AUTH_SIGNOUTUSERREQUEST:
      case PROS_GLOBAL_API_AUTH_SIGNOUTUSERRESPONSE:    return HandleSignOut(u);

      case PROS_API_FACTS_WRITEBINARYPACKUSERREQUEST:   return HandleFactsWriteBinaryPackUser(u);
   }

   // -- party
   switch (u.reqNameId) {
      case HYDRA_API_PUSH_PRESENCE_PRESENCEPARTYUPDATE: return HandlePartyUpdate(u);
      case HYDRA_API_PRESENCE_PARTYINVITEACCEPTREQUEST: return HandlePartyInviteAcceptRequest(u);
      case HYDRA_API_PRESENCE_PARTYDISBANDREQUEST:      return HandlePartyDisbandRequest(u);
   }

   // -- MM
   switch (u.reqNameId) {
      case HYDRA_API_PUSH_PRESENCE_PRESENCESESSIONUPDATE:      return HandleMMSessionUpdate(u);
      case HYDRA_API_PRESENCE_MATCHMAKESESSIONLEAVEREQUEST:    return HandleMatchmakeSessionLeaveRequest(u);
   }

   // -- servers
   return ApplyServerReducers(u);
}

bool StartServerController::ApplyServerReducers(SdkPacket& u)
{
   // Dedicated servers
   switch (u.reqNameId) {
      case HYDRA_API_DEDICATEDSERVERS_DSDSMCOMMUNICATION_HANDSHAKEREQUEST:
      case HYDRA_API_DEDICATEDSERVERS_DSDSMCOMMUNICATION_HANDSHAKERESPONSE:      return HandleDedicatedServerHandshake(u);
      case HYDRA_API_DEDICATEDSERVERS_DSDSMCOMMUNICATION_GETSERVERSESSIONINFOREQUEST:
      case HYDRA_API_DEDICATEDSERVERS_DSDSMCOMMUNICATION_GETSERVERSESSIONINFORESPONSE: return HandleDedicatedServerSessionInfo(u);
   }

   // Standalone servers
   switch (u.reqNameId) {
      case PROS_GLOBAL_API_AUTH_GETSTANDALONESIGNINCODEREQUEST:   return HandleGetStandaloneSignInCodeRequest(u);
      case PROS_GLOBAL_API_AUTH_GETSTANDALONESIGNINCODERESPONSE:  return HandleGetStandaloneSignInCodeResponse(u);
      case PROS_GLOBAL_API_AUTH_SIGNINSTANDALONECODEREQUEST:      return HandleSignInStandaloneCodeRequest(u);
      case PROS_GLOBAL_API_AUTH_SIGNINSTANDALONECODERESPONSE:     return HandleSignInStandaloneCodeResponse(u);
      case HYDRA_API_SESSIONCONTROL_CREATESERVERREQUEST:          return HandleCreateStandaloneServerRequest(u);
      case HYDRA_API_SESSIONCONTROL_CREATESERVERRESPONSE:         return HandleCreateStandaloneServerResponse(u);
      case HYDRA_API_SESSIONCONTROL_HEARTBEATSERVERREQUEST:       return HandleStandaloneHeartbeatServerRequest(u);
      case HYDRA_API_CHALLENGES_SERVERGETCHALLENGESREQUEST:       return HandleStandaloneServerGetChallengesRequest(u);
   }

   return false;
}

bool StartServerController::ApplyReorderCommands()
{
   // Apply Inspector reorder intent (layout-only). This must trigger a reproject
   // even when there are no new packets.
   {
      // NOTE: adjust this path if your GraphViewState lives somewhere else.
      // In your UI, InspectorPanel already holds a GraphViewState&; this should be the same instance.
      GraphViewState& view = mainModel->view;

      const int delta = view.pendingUpDownDelta;
      if (delta != 0) {
         view.pendingUpDownDelta = 0;

         const NodeId sel = view.selected.node;
         const GraphNode* n = mainModel->graph.Find(sel);
         if (n) {
            return st.MoveEntityInOrder(n->kind, n->entityKey, delta);
         }
      }
   }

   return false;
}

} // namespace Sample::UI::Controllers
