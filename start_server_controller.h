#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <deque>
#include <string>
#include <optional>
#include <thread>
#include <atomic>

#include "httplib.h"
#include "json.hpp"

#include "ui/models/start_server_model.h"
#include "connectors/backend_connector.h"
#include "start_server_controller_interface.h"
#include "ui/views/window_interface.h"

#pragma message("start_server_controller.h REV: Graph nodes v0.2")

namespace Sample::UI::Controllers {

struct CoordinatorUpdate {
   nlohmann::json payload;
   std::string printable; // for logs/debug display
   double recv_time_s = 0.0;
};

class CoordinatorHttpServer {
public:
   CoordinatorHttpServer(Models::MainModel* model);
   ~CoordinatorHttpServer() { Stop(); }

   void Start(const char* host = "127.0.0.1", int port = 30001);
   void Stop();

   // Called from UI thread (TickIngestion) to drain events
   bool TryPop(CoordinatorUpdate& out);

private:
   void ServerThreadMain(std::string host, int port);

private:
   std::mutex mtx_;
   std::deque<CoordinatorUpdate> queue_;

   std::thread thr_;
   std::atomic<bool> running_{ false };

   // Owned by server thread
   httplib::Server* svr_ = nullptr;
   Models::MainModel* mainModel = nullptr;
};

class StartServerController : public StartServerControllerInterface {
public:
   StartServerController(Models::MainModel* model, Sample::Connector::BackendConnectorInterface* connector);
   bool StartServer(const Models::ServerType serverType) override;
   void TickIngestion() override;
   void StopServer() override;
   void SetStartServerServiceState(const UI::Models::ServiceState& serviceState) override;
   UI::Models::ServiceState GetServiceState() const override;

private:
   Sample::Connector::BackendConnectorInterface* backendConnector;
   Models::StartServerModel startServerModel;
   Models::MainModel* mainModel = nullptr;

   CoordinatorHttpServer  server;
};


} // namespace Sample::UI::Controllers
