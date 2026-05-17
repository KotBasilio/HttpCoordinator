#pragma once

#include <memory>

#include "connectors/backend_connector.h"
#include "start_server_controller_interface.h"
#include "live_state.h"

namespace Sample::UI::Controllers {

class CoordinatorHttpServer;
struct SdkPacket;

class StartServerController : public StartServerControllerInterface 
{
public:
   StartServerController(Models::MainModel* model, Sample::Connector::BackendConnectorInterface* connector);
   ~StartServerController();
   bool StartServer(const Models::ServerType serverType) override;
   void TickIngestion() override;
   void StopServer() override;
   void SetStartServerServiceState(const UI::Models::ServiceState& serviceState) override;
   UI::Models::ServiceState GetServiceState() const override;

private:
   Sample::Connector::BackendConnectorInterface* backendConnector;
   Models::StartServerModel startServerModel;
   Models::MainModel* mainModel = nullptr;

   std::unique_ptr<CoordinatorHttpServer> server;
   LiveState st;
   StandaloneCorrelationState standaloneCorr;

   // packets ingestion -- expands TickIngestion()
   void SwitchGraphToIngestion();
   bool ApplyAllReducers(SdkPacket& u);
   bool ApplyServerReducers(SdkPacket& u);
   bool HandleSignIn(SdkPacket& u);
   bool HandleSignOut(SdkPacket& u);
   bool HandlePartyUpdate(SdkPacket& u);
   bool HandlePartyInviteAcceptRequest(SdkPacket& u);
   bool HandlePartyDisbandRequest(SdkPacket& u);
   bool HandleMMSessionUpdate(SdkPacket& u);
   bool HandleMMSessionMembers(SdkPacket& u, SessionState& sess);
   bool HandleFactsWriteBinaryPackUser(SdkPacket& u);
   bool HandleMatchmakeSessionLeaveRequest(SdkPacket& u);
   bool HandleDedicatedServerHandshake(SdkPacket& u);
   bool HandleDedicatedServerSessionInfo(SdkPacket& u);
   bool HandleGetStandaloneSignInCodeRequest(SdkPacket& u);
   bool HandleGetStandaloneSignInCodeResponse(SdkPacket& u);
   bool HandleSignInStandaloneCodeRequest(SdkPacket& u);
   bool HandleSignInStandaloneCodeResponse(SdkPacket& u);
   bool HandleCreateStandaloneServerRequest(SdkPacket& u);
   bool HandleCreateStandaloneServerResponse(SdkPacket& u);
   bool HandleStandaloneHeartbeatServerRequest(SdkPacket& u);
   bool HandleStandaloneServerGetChallengesRequest(SdkPacket& u);
   bool ApplyReorderCommands();

   // projection LiveState -> GraphModel
   void ProjectToGraph();
   void ProjectHydraUsers();
   void ProjectMMSessions();
   void ProjectParties();
   void ProjectServers();
   void UpdateColumnsInLayout();
   void ReduceLinksPartyOwnsSession();
};

} // namespace Sample::UI::Controllers
