#pragma once

#include <deque>

#include "httplib.h"
#include "json.hpp"

#include "ui/models/main_model.h"
#include "ui/models/ReqNamesEnum.h"

namespace Sample::UI::Controllers {

struct SdkPacket {
   nlohmann::json payload;
   double recv_time_s = 0.0;
   bool isEmptyBody = false;
   ReqNameID reqNameId = ABSENT_REQNAME_ID;

   // for logs/debug display
   std::string printable;
   std::string redacted;

   SdkPacket() = default;
   SdkPacket(const httplib::Request& req);
   static void ValidateKeysArray();
private:
   SdkPacket(const std::string& reqBody);
   std::string RedactSecrets(const std::string& reqBody);
};

class CoordinatorHttpServer {
public:
   CoordinatorHttpServer(Models::MainModel* model);
   ~CoordinatorHttpServer() { Stop(); }

   void Start(const char* host = "127.0.0.1", int port = 30001);
   void Stop();

   // Called from UI thread (TickIngestion) to drain events
   bool TryPop(SdkPacket& out);

private:
   void ServerThreadMain(std::string host, int port);
   void HandlePostCoordinator(const httplib::Request& req, httplib::Response& res);

private:
   Models::MainModel* mainModel = nullptr;

   std::mutex mtx_;
   std::deque<SdkPacket> queue_;

   std::thread thr_;
   std::atomic<bool> running_{ false };

   // owned by CoordinatorHttpServer
   std::mutex svr_mtx_;
   std::unique_ptr<httplib::Server> svr_;
};

} // namespace Sample::UI::Controllers
