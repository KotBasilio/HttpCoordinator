#include "ui/controllers/start_server_controller.h"
#include "ui/controllers/coordinator_http_server.h"
#include "packet_json_helpers.h"

#include <algorithm>

#pragma message("users_ingestion.cpp REV: SC sessions v0.1")

namespace Sample::UI::Controllers
{

using namespace nlohmann::literals; // enables "... "_json_pointer

bool StartServerController::HandleFactsWriteBinaryPackUser(SdkPacket& u)
{
   const auto& p = u.payload;

   // IMPORTANT: bind facts to USER_ID (Presence userId), not userIdentity.
   // Facts packets carry USER_ID in header/context.
   std::string uid = ExtractFactsHeaderValue(p, "USER_ID");
   if (uid.empty()) {
      uid = ExtractUserIdentity(p); // fallback
   }
   if (uid.empty()) return false;

   st.TouchUser(uid);
   auto& usr = st.users[uid];

   // Copy all propertyName/propertyValue pairs from header/context.
   static const auto PCTX = "/header/context"_json_pointer;
   const Json* ctx = NodeAt(p, PCTX);
   if (!ctx || !ctx->is_array()) return false;

   auto backup = usr.facts;
   usr.facts.clear();
   usr.facts.reserve(ctx->size());

   for (const auto& e : *ctx) {
      if (!e.is_object()) continue;
      const std::string k = GetStr(e, "propertyName", "");
      if (k.empty()) continue;
      std::string v = GetStr(e, "propertyValue", "");

      // nickname strangeness (possible SDK bug): when we have two users logged-in, ACCOUNT_NAME sometimes comes from the other user.
      if (k == "ACCOUNT_NAME") {
         if (usr.nickname.empty() && !v.empty()) {
            usr.nickname = v; // bootstrap
         } else {
            v = usr.nickname;
         }
      }

      usr.facts.emplace_back(k, v);
   }

   // Merge backup into facts -- favor facts.
   for (const auto& [key, value] : backup) {
      auto it = std::find_if(usr.facts.begin(), usr.facts.end(),
         [&key](const auto& pair) { return pair.first == key; });

      if (it == usr.facts.end()) {
         usr.facts.emplace_back(key, value);
      }
   }

   return true;
}

bool StartServerController::HandleSignIn(SdkPacket& u)
{
   const auto& p = u.payload;
   const std::string uid = ExtractUserIdentity(p);
   if (uid.empty()) {
      return false;
   }

   st.users[uid].online = true;
   st.TouchUser(uid);
   const std::string hydraKernelSessionId = ExtractHydraKernelSessionId(p);
   if (!hydraKernelSessionId.empty())
      st.users[uid].hydraKernelSessionId = hydraKernelSessionId;

   return true;
}

bool StartServerController::HandleSignOut(SdkPacket& u)
{
   const auto& p = u.payload;
   const std::string uid = ExtractUserIdentity(p);
   if (uid.empty()) {
      return false;
   }

   st.users[uid].online = false;
   st.TouchUser(uid);
   st.UnbindUser(uid);

   return true;
}

} // namespace Sample::UI::Controllers
