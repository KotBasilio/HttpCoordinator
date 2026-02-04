#include "ui/controllers/start_server_controller.h"
#include "ui/models/start_server_model.h"

#pragma message("start_server_controller.cpp REV: Graph nodes v0.2")

static void HttplibJsonSmokeTest()
{
   httplib::Server svr;
   nlohmann::json j = { {"hello", "world"} };
   (void)svr; (void)j;
}

static double NowSeconds()
{
   using clock = std::chrono::steady_clock;
   static const auto t0 = clock::now();
   auto dt = std::chrono::duration<double>(clock::now() - t0);
   return dt.count();
}

static std::string GetStr(const nlohmann::json& obj, const char* key, const std::string& fallback)
{
   auto it = obj.find(key);
   if (it == obj.end() || it->is_null())
      return fallback;

   if (it->is_string())          return it->get<std::string>();
   if (it->is_number_integer())  return std::to_string(it->get<long long>());
   if (it->is_number_unsigned()) return std::to_string(it->get<unsigned long long>());
   if (it->is_number_float())    return std::to_string(it->get<double>());
   if (it->is_boolean())         return it->get<bool>() ? "true" : "false";

   // For objects/arrays: todo later
   return fallback;
}

static GraphNode& UpsertNode(GraphModel& g, NodeId id)
{
   // Linear search is fine at our current scale; we rebuild index at the end anyway.
   for (auto& n : g.nodes)
      if (n.id == id)
         return n;

   g.nodes.push_back({});
   auto& n = g.nodes.back();
   n.id = id;
   n.kind = NodeKind::Unknown;
   n.title.clear();
   n.subtitle.clear();
   n.entityKey.clear();
   n.pos = ImVec2(0, 0);
   n.size = ImVec2(160, 70);
   return n;
}

static void EnsureLink(GraphModel& g, NodeId from, NodeId to,
   GraphPort fromPort = GraphPort::Right, GraphPort toPort = GraphPort::Left)
{
   for (const auto& e : g.links)
      if (e.from == from && e.to == to)
         return;

   GraphLink L{};
   L.from = from;
   L.to = to;
   L.fromPort = fromPort;
   L.toPort = toPort;
   L.style = LinkStyle::Bezier;
   L.arrow = true;
   g.links.push_back(L);
}


namespace Sample::UI::Controllers {
CoordinatorHttpServer::CoordinatorHttpServer(Models::MainModel* model)
{
   mainModel = model;
}

void CoordinatorHttpServer::Start(const char* host, int port)
{
   if (running_.exchange(true)) return; // already running

   thr_ = std::thread([this, host_s = std::string(host), port]() {
      ServerThreadMain(host_s, port);
      });
}

void CoordinatorHttpServer::Stop()
{
   if (!running_.exchange(false)) return;

   // Stop server (thread-safe in httplib)
   if (svr_) svr_->stop();

   if (thr_.joinable())
      thr_.join();
}

bool CoordinatorHttpServer::TryPop(CoordinatorUpdate& out)
{
   std::lock_guard<std::mutex> lock(mtx_);
   if (queue_.empty()) return false;
   out = std::move(queue_.front());
   queue_.pop_front();
   return true;
}

void CoordinatorHttpServer::ServerThreadMain(std::string host, int port)
{
   httplib::Server svr;
   svr_ = &svr;

   if (!mainModel) {
      return;
   }

   auto handler = [this](const httplib::Request& req, httplib::Response& res) {
      try {
         auto I = [&](const char* s) { mainModel->logs.Push(UI::Models::LogLevel::Info, std::string(s)); };
         auto j = nlohmann::json::parse(req.body);

         CoordinatorUpdate u;
         u.payload = std::move(j);
         u.recv_time_s = NowSeconds();

         // Pretty-print for display; keep it bounded so logs don't explode
         u.printable = u.payload.dump(2); // indent=2
         constexpr size_t kMaxLogChars = 4000;
         if (u.printable.size() > kMaxLogChars) {
            u.printable.resize(kMaxLogChars);
            u.printable += "\n...<truncated>...";
         }

         I("Received coordinator packet with payload");
         mainModel->logs.Push(UI::Models::LogLevel::Info, u.printable, u.recv_time_s);

         {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push_back(std::move(u));
            if (queue_.size() > 2000) queue_.pop_front();
         }

         res.status = 200;
         res.set_content("OK\n", "text/plain");
      }
      catch (const std::exception& e) {
         res.status = 400;
         res.set_content(std::string("Bad JSON: ") + e.what() + "\n", "text/plain");
      }
   };

   // Simple endpoint: POST '/coordinator' with JSON body
   svr.Post("/coordinator", handler);

   // A tiny health check helps debugging
   svr.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
      auto I = [&](const char* s) { mainModel->logs.Push(UI::Models::LogLevel::Info, std::string(s)); };
      res.status = 200;
      res.set_content("alive\n", "text/plain");

      I("Health check");
   });

   // This blocks until stop() is called
   svr.listen(host.c_str(), port);

   svr_ = nullptr;
}

StartServerController::StartServerController(Models::MainModel* model, Sample::Connector::BackendConnectorInterface* connector)
   : backendConnector(connector)
   , server(model)
   , mainModel(model)
{
   //HttplibJsonSmokeTest();
}


bool StartServerController::StartServer(const Models::ServerType serverType)
{
   server.Start();
   return true;
}

void StartServerController::StopServer()
{
   server.Stop();
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

   CoordinatorUpdate u;
   bool any = false;

   while (server.TryPop(u)) {
      any = true;

      auto& logs = mainModel->logs;
      auto& graph = mainModel->graph;

      const auto& p = u.payload;
      if (!p.is_object()) {
         logs.Push(Models::LogLevel::Warn, "coordinator: payload is not a JSON object");
         continue;
      }

      // First packet: switch graph to ingestion-driven (wipe any demo seed created elsewhere)
      if (!mainModel->graphUsesIngestion) {
         mainModel->graphUsesIngestion = true;
         graph.nodes.clear();
         graph.links.clear();
         graph.indexById.clear();
         logs.Push(Models::LogLevel::Info, "coordinator: first packet -> switching graph to ingestion-driven");
      }

      constexpr NodeId AUTH_ID = 1001;
      constexpr NodeId ACCOUNT_ID = 1002;

      bool haveAuth = false;
      bool haveAcct = false;

      // authState
      auto itA = p.find("authState");
      if (itA != p.end() && itA->is_object()) {
         const auto& a = *itA;
         const std::string serviceState = GetStr(a, "serviceState", "<unknown>");
         const std::string id = GetStr(a, "id", "<no-id>");
         const std::string provider = GetStr(a, "provider", "<unknown>");
         const std::string platform = GetStr(a, "platform", "<unknown>");

         logs.Push(Models::LogLevel::Info,
            "coordinator: authState State=" + serviceState +
            " Id=" + id + " Provider=" + provider + " Platform=" + platform);

         auto& n = UpsertNode(graph, AUTH_ID);
         n.kind = NodeKind::Unknown;
         n.title = "Auth";
         n.subtitle = serviceState + " | " + id;
         n.entityKey = "auth";
         n.pos = ImVec2(0, 0);
         n.size = ImVec2(160, 70);

         haveAuth = true;
      }

      // accountStatus
      auto itS = p.find("accountStatus");
      if (itS != p.end() && itS->is_object()) {
         const auto& s = *itS;
         const std::string linkState = GetStr(s, "linkState", "<unknown>");
         const std::string systemMode = GetStr(s, "systemMode", "<unknown>");

         logs.Push(Models::LogLevel::Info,
            "coordinator: accountStatus LinkState=" + linkState +
            " SystemMode=" + systemMode);

         auto& n = UpsertNode(graph, ACCOUNT_ID);
         n.kind = NodeKind::Unknown;
         n.title = "Account";
         n.subtitle = linkState + " | " + systemMode;
         n.entityKey = "account";
         n.pos = ImVec2(240, 0);
         n.size = ImVec2(200, 70);

         haveAcct = true;
      }

      // Add link when both exist (now enabled)
      if (haveAuth && haveAcct) {
         EnsureLink(graph, AUTH_ID, ACCOUNT_ID);
      }
   }

   if (any) {
      mainModel->graph.RebuildIndex();
   }
}

} // namespace Sample::UI::Controllers
