#define REQ_NAMES_PARSER_IMPL

#include "ui/controllers/coordinator_http_server.h"

#pragma message("coordinator_http_server.cpp REV: SC sessions v0.3")

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

// ParseReqName: returns the matching ReqNameID, or ABSENT_REQNAME_ID if not found.
// ReqNameKey[i] corresponds to enum value (i)
static ReqNameID ParseReqName(const std::string& key)
{
   for (size_t i = 0; i < ARRAY_SIZE(ReqNameKey); ++i) {
      if (std::strcmp(ReqNameKey[i], key.c_str()) == 0) {
         return static_cast<ReqNameID>(i);
      }
   }
   return ABSENT_REQNAME_ID;
}

static double NowSeconds()
{
   using clock = std::chrono::steady_clock;
   static const auto t0 = clock::now();
   auto dt = std::chrono::duration<double>(clock::now() - t0);
   return dt.count();
}

namespace Sample::UI::Controllers {

void SdkPacket::ValidateKeysArray() 
{
   // Size-check: ensure the generated array size matches the enum count.
   static_assert(
      (ARRAY_SIZE(ReqNameKey)) == (size_t)(REQNAMEID_COUNT),
      "ReqNameKey size mismatch: generated array must match enum entries"
   );

   // Spot-check: each enum value maps back to its string
   for (size_t i = 0; i < (REQNAMEID_COUNT - 1); ++i) {
      ReqNameID id = static_cast<ReqNameID>(i);
      const char* key = ReqNameKey[i];
      assert(ParseReqName(key) == id && "ParseReqName mismatch");
   }

   // Negative test: unknown key returns ABSENT_REQNAME_ID
   assert(ParseReqName("Nonexistent.Key") == ABSENT_REQNAME_ID);

   OutputDebugString("Validated pipeline: Request Name->ID\n");
}

std::string SdkPacket::RedactSecrets(const std::string& reqBody)
{
   std::string redactedBody = reqBody;
   size_t pos = 0;

   // Search for strings marked with quotes
   bool isNextToken = false;
   bool isNextData = false;
   while ((pos = redactedBody.find('"', pos)) != std::string::npos) {
      // get value
      size_t start = pos + 1;
      size_t end = redactedBody.find('"', start);
      if (end == std::string::npos) { // No closing quote found
         break; 
      }
      std::string value = redactedBody.substr(start, end - start);

      // it contains "==" or ends with "=" => secret
      bool keep = !isNextToken;
      if (keep) {
         bool isSecret = (value.find("==") != std::string::npos);
         isSecret |= (value.length() > 3) && (value.back() == '=');
         keep = !isSecret;
      }

      // long data
      if (isNextData && value.length() > 100) {
         keep = false;
      }

      // may cut
      if (!keep) {
         redactedBody.replace(start, end - start, "(cut)");
         end = redactedBody.find('"', start);
      }

      // detect next string type by heuristics
      static const std::array<std::string, 6> heuristics = {
         "token",
         "serverToken",
         "hash",
         "hashKey",
         "signature", 
         "externalIdentityToken"
      };
      isNextToken = std::find(heuristics.begin(), heuristics.end(), value) != heuristics.end();
      isNextData = (value == "data") || (value == "entries");

      // Move past the closing quote
      pos = end + 1; 
   }

   return redactedBody;
}

SdkPacket::SdkPacket(const std::string& reqBody)
{
   loggable = RedactSecrets(reqBody);
   payload = nlohmann::json::parse(loggable);
   recv_time_s = NowSeconds();
}

SdkPacket::SdkPacket(const httplib::Request& req)
   : SdkPacket(req.body)
{
   std::string reqName = req.has_header("Request-Name") ? req.get_header_value("Request-Name") : "<unk.req>";
   reqNameId = ParseReqName(reqName);

   printable = reqName;
   if (req.body.length() > 5) {
      printable += "|" + req.body.substr(0, 70) + "...";
   } else {
      isEmptyBody = true;
   }

   if (reqNameId == ABSENT_REQNAME_ID) {
      OutputDebugString((reqName + " -- add to ReqNames.txt\n").c_str());
   } else {
      OutputDebugString((reqName + "|" + loggable + "\n").c_str());
   }
}

CoordinatorHttpServer::CoordinatorHttpServer(Models::MainModel* model)
{
   mainModel = model;
   SdkPacket::ValidateKeysArray();
}

bool CoordinatorHttpServer::TryPop(SdkPacket& out)
{
   std::lock_guard<std::mutex> lock(mtx_);
   if (queue_.empty()) return false;
   out = std::move(queue_.front());
   queue_.pop_front();
   return true;
}

void CoordinatorHttpServer::HandlePostCoordinator(const httplib::Request& req, httplib::Response& res)
{
   SdkPacket u(req);
   {
      std::lock_guard<std::mutex> lock(mtx_);
      mainModel->logs.Info(u.printable);
      queue_.push_back(std::move(u));
      if (queue_.size() > 2000) queue_.pop_front();
   }
}

void CoordinatorHttpServer::Start(const char* host, int port)
{
   if (running_.exchange(true)) 
      return; // already running

   {
      std::lock_guard<std::mutex> lk(svr_mtx_);
      svr_ = std::make_unique<httplib::Server>();
   }

   thr_ = std::thread([this, host_s = std::string(host), port]() {
      ServerThreadMain(host_s, port);
   });
}

void CoordinatorHttpServer::Stop()
{
   if (!running_.exchange(false)) return;

   // Stop server (with mutex)
   {
      std::lock_guard<std::mutex> lk(svr_mtx_);
      if (svr_) svr_->stop();
   }

   if (thr_.joinable()) thr_.join();

   // Cleanup server (with mutex)
   {
      std::lock_guard<std::mutex> lk(svr_mtx_);
      svr_.reset();
   }
}

void CoordinatorHttpServer::ServerThreadMain(std::string host, int port)
{
   if (!mainModel) return;

   httplib::Server* svr = nullptr;
   {
      std::lock_guard<std::mutex> lk(svr_mtx_);
      svr = svr_.get();
   }
   if (!svr) return;

   // If Stop() happened before we even got here, don't block in listen.
   if (!running_.load()) return;

   // POST '/coordinator' endpoint: with JSON body
   auto handler = [this](const httplib::Request& req, httplib::Response& res) {
      try {
         HandlePostCoordinator(req, res);
         res.status = 200;
         res.set_content("OK\n", "text/plain");
      }
      catch (const nlohmann::json::parse_error& e) {
         // Bad JSON syntax
         res.status = 400;
         res.set_content(std::string("Bad JSON: ") + e.what() + "\n", "text/plain");
      }
      catch (const nlohmann::json::exception& e) {
         // Other JSON-related issues (type errors, etc.)
         res.status = 400;
         res.set_content(std::string("JSON error: ") + e.what() + "\n", "text/plain");
      }
      catch (const std::exception& e) {
         // Everything else
         res.status = 500;
         res.set_content(std::string("Server error: ") + e.what() + "\n", "text/plain");
      }
   };
   svr->Post("/coordinator", handler);

   // GET '/health' endpoint: helps debugging
   svr->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
      auto I = [&](const char* s) { mainModel->logs.Info(std::string(s)); };
      res.status = 200;
      res.set_content("alive\n", "text/plain");
      I("Health check");
   });

   // This blocks until stop() is called
   svr->listen(host.c_str(), port);
}

} // namespace Sample::UI::Controllers

