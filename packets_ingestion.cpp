#include "ui/controllers/start_server_controller.h"
#include "ui/controllers/coordinator_http_server.h"
#include "ui/models/start_server_model.h"

#pragma message("packets_ingestion.cpp REV: SC sessions v0.0")

namespace Sample::UI::Controllers 
{

using namespace nlohmann::literals; // enables "... "_json_pointer
using json = nlohmann::json;
using jptr = json::json_pointer;

static const json* NodeAt(const json& j, const jptr& ptr) noexcept
{
   // contains(ptr) is "safe existence check" for pointer paths
   // (no insertion, no out_of_range).
   if (!j.contains(ptr)) return nullptr;
   return &j.at(ptr);
}

// useful for one-key lookups where we want to accept string/number/bool and stringify it.
static std::string GetStr(const json& obj, const char* key, std::string_view fallback = {})
{
   auto it = obj.find(key);
   if (it == obj.end() || it->is_null())
      return std::string(fallback);

   if (it->is_string())          return it->get<std::string>();
   if (it->is_number_integer())  return std::to_string(it->get<long long>());
   if (it->is_number_unsigned()) return std::to_string(it->get<unsigned long long>());
   if (it->is_number_float())    return std::to_string(it->get<double>());
   if (it->is_boolean())         return it->get<bool>() ? "true" : "false";
   // For objects/arrays: todo later

   return std::string(fallback);
}

// Equivalent spirit to GetStr(): accept string/number/bool and stringify.
static std::string StrAt(const json& j, const jptr& ptr, std::string_view fallback = {})
{
   auto* n = NodeAt(j, ptr);
   if (!n || n->is_null()) return std::string(fallback);

   if (n->is_string())          return n->get<std::string>();
   if (n->is_number_integer())  return std::to_string(n->get<long long>());
   if (n->is_number_unsigned()) return std::to_string(n->get<unsigned long long>());
   if (n->is_number_float())    return std::to_string(n->get<double>());
   if (n->is_boolean())         return n->get<bool>() ? "true" : "false";
   // For objects/arrays: todo later

   return std::string(fallback);
}

// When we only want actual strings: StringAt + value_or.
static std::optional<std::string> StringAt(const json& j, const jptr& ptr)
{
   if (auto* n = NodeAt(j, ptr); n && n->is_string())
      return n->get<std::string>();
   return std::nullopt;
}

static bool BoolAt(const json& j, const jptr& ptr, bool fallback = false) noexcept
{
   if (auto* n = NodeAt(j, ptr); n && n->is_boolean())
      return n->get<bool>();
   return fallback;
}

static std::string ExtractUserIdentity(const json& p)
{
   static const auto P1 = "/data/userContext/data/userIdentity"_json_pointer;
   static const auto P2 = "/userContext/data/userIdentity"_json_pointer;
   static const auto P3 = "/context/data/userIdentity"_json_pointer;

   if (auto s = StringAt(p, P1)) return std::move(*s);
   if (auto s = StringAt(p, P2)) return std::move(*s);
   if (auto s = StringAt(p, P3)) return std::move(*s);
   return {};
}

static std::string ExtractSessionId(const json& p)
{
   // PresenceSessionUpdate carries: id.id
   static const auto PID = "/id/id"_json_pointer;
   return StrAt(p, PID, "");
}

static std::string ExtractNicknameFromStaticData(const json& memberData)
{
   // memberData.staticData is usually a JSON string; best effort parse.
   std::string sd = GetStr(memberData, "staticData", "");
   if (sd.empty()) return "";

   // Sometimes it is already cut or not JSON; ignore quietly.
   if (sd.size() < 2 || sd.front() != '{') return "";

   auto j = json::parse(sd, nullptr, false);
   if (!j.is_discarded()) { 
      auto it = j.find("nickname");
      if (it != j.end() && it->is_string())
         return it->get<std::string>();
   }
   return "";
}

static std::string ExtractFactsHeaderValue(const json& p, const char* key)
{
   static const auto PCTX = "/header/context"_json_pointer;
   const json* ctx = NodeAt(p, PCTX);
   if (!ctx || !ctx->is_array()) return {};

   for (const auto& e : *ctx) {
      if (!e.is_object()) continue;
      const std::string name = GetStr(e, "propertyName", "");
      if (name == key) {
         return GetStr(e, "propertyValue", "");
      }
   }
   return {};
}

bool StartServerController::HandleFactsWriteBinaryPackUser(SdkPacket& u)
{
   const auto& p = u.payload;

   // IMPORTANT: bind facts to USER_ID (Presence userId), not userIdentity
   // Facts packets carry USER_ID in header/context.
   std::string uid = ExtractFactsHeaderValue(p, "USER_ID");
   if (uid.empty()) {
      uid = ExtractUserIdentity(p); // fallback
   }
   if (uid.empty()) return false;

   st.TouchUser(uid);
   auto& usr = st.users[uid];

   // Copy all propertyName/propertyValue pairs from header/context
   static const auto PCTX = "/header/context"_json_pointer;
   const json* ctx = NodeAt(p, PCTX);
   if (!ctx || !ctx->is_array()) return false;

   auto backup = usr.facts;
   usr.facts.clear();
   usr.facts.reserve(ctx->size());

   for (const auto& e : *ctx) {
      if (!e.is_object()) continue;
      const std::string k = GetStr(e, "propertyName", "");
      if (k.empty()) continue;
      std::string v = GetStr(e, "propertyValue", "");

      // nickname strangeness (possible SDK bug): when we have two users logged-in, ACCOUNT_NAME sometimes comes from the other user
      if (k == "ACCOUNT_NAME") {
         if (usr.nickname.empty() && !v.empty()) {
            usr.nickname = v; // bootstrap
         } else {
            v = usr.nickname; // this way
         }
      }

      usr.facts.emplace_back(k, v);
   }

   // Merge backup into facts -- favor facts
   for (const auto& [key, value] : backup) {
      auto it = std::find_if(usr.facts.begin(), usr.facts.end(),
         [&key](const auto& pair) { return pair.first == key; });

      if (it == usr.facts.end()) {
         usr.facts.emplace_back(key, value);
      }
   }

   return true;
}

bool StartServerController::HandleMatchmakeSessionLeaveRequest(SdkPacket& u)
{
   const auto& p = u.payload;

   // Request contains userIdentity in context.data.userIdentity (as in your sample).
   const std::string uid = ExtractUserIdentity(p);
   if (uid.empty())
      return false;

   // Ensure user exists in state; leaving MM != going offline
   st.TouchUser(uid);
   st.users[uid].online = true;

   // Remove membership from all sessions (no sessionId given here).
   bool removedAny = false;
   for (auto& skv : st.sessions) {
      auto& sess = skv.second;
      if (sess.members.erase(uid) > 0) {
         removedAny = true;
      }
   }

   return removedAny;
}

// Sign-in / Sign-out: flip online flag; keep HydraSample per user always
bool Controllers::StartServerController::HandleSignIn(SdkPacket& u)
{
   const auto& p = u.payload;
   const std::string uid = ExtractUserIdentity(p);
   if (uid.empty()) {
      return false;
   }

   st.users[uid].online = true;
   st.TouchUser(uid);

   return true;
}

bool Controllers::StartServerController::HandleSignOut(SdkPacket& u)
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

// PresenceSessionUpdate is the truth stream for MM state + membership
bool Controllers::StartServerController::HandleMMSessionUpdate(SdkPacket& u)
{
   bool changed = false;
   const auto& p = u.payload;

   // get to session id
   const std::string sessionId = ExtractSessionId(p);
   if (sessionId.empty()) {
      return false;
   }
   st.TouchSession(sessionId);
   auto& sess = st.sessions[sessionId];

   // state string
   std::string mmState = GetStr(p, "state", "");
   if (!mmState.empty() && mmState != sess.mmState) {
      sess.mmState = std::move(mmState);
      changed = true;
   }

   // members deltas
   changed |= HandleMMSessionMembers(u, sess);

   return changed;
}

bool StartServerController::HandleMMSessionMembers(SdkPacket& u, SessionState& sess)
{
   const auto& p = u.payload;
   bool changed = false;

   // get to the array
   auto itGD = p.find("gameData");
   if (itGD == p.end() || !itGD->is_object()) {
      return changed;
   }
   auto itMU = itGD->find("membersUpdate");
   if (itMU == itGD->end() || !itMU->is_array()) {
      return changed;
   }

   // membersUpdate deltas
   for (const auto& mu : *itMU) {
      if (!mu.is_object()) continue;

      auto itMD = mu.find("memberData");
      if (itMD == mu.end() || !itMD->is_object()) continue;

      const auto& md = *itMD;
      const std::string uid = GetStr(md, "userId", "");
      if (uid.empty()) continue;

      st.TouchUser(uid);
      changed = true;

      // nickname best effort
      auto& usr = st.users[uid];
      if (usr.nickname.empty()) {
         std::string nick = ExtractNicknameFromStaticData(md);
         if (!nick.empty()) usr.nickname = std::move(nick);
      }

      // operations
      const std::string op = GetStr(mu, "updateType", "");
      if (op == "PRESENCE_SESSION_MEMBER_UPDATE_TYPE_ADD") {
         SessionState::MemberInfo info{};
         info.groupId = GetStr(md, "groupId", "");
         info.isOwner = BoolAt(md, "/isOwner"_json_pointer, false);
         sess.members[uid] = std::move(info);
         usr.online = true;
      } else if (op == "PRESENCE_SESSION_MEMBER_UPDATE_TYPE_REMOVE") {
         sess.members.erase(uid);
      } else if (op == "PRESENCE_SESSION_MEMBER_UPDATE_TYPE_UPDATE") {
         auto& info = sess.members[uid]; // create if missing
         info.groupId = StrAt(md, "/groupId"_json_pointer, "");
         info.isOwner = BoolAt(md, "/isOwner"_json_pointer, info.isOwner);
      } else {
         OutputDebugString((op + " -- to handle as MMSessionMembers operation\n").c_str());
      }
   }

   return changed;
}

bool StartServerController::HandlePartyUpdate(SdkPacket& u)
{
   const auto& p = u.payload;

   // Party id is normally here
   static const auto PID = "/id/id"_json_pointer;
   const json* jPid = NodeAt(p, PID);
   const std::string partyId = (jPid && jPid->is_string()) ? jPid->get<std::string>() : "";

   if (partyId.empty()) {
      // Some updates are reason-only; skip for now (we can map later if needed)
      return false;
   }

   st.TouchParty(partyId);
   auto& party = st.parties[partyId];

   bool changed = false;

   // Join code (optional)
   {
      static const auto JC = "/joinCode/data"_json_pointer;
      std::string jc = StrAt(p, JC, "");
      if (!jc.empty() && jc != party.joinCode) {
         party.joinCode = jc;
         changed = true;
      }
   }

   // Members update stream (delta)
   static const auto MU = "/membersUpdate"_json_pointer;
   const json* jMu = NodeAt(p, MU);
   if (!jMu) {
      static const auto MU1 = "/members"_json_pointer;
      jMu = NodeAt(p, MU1);
   }

   if (jMu && jMu->is_array()) {
      for (const auto& e : *jMu) {
         if (!e.is_object()) continue;

         const std::string ut = GetStr(e, "updateType", "");
         const json* jm = NodeAt(e, "/member"_json_pointer);
         if (!jm || !jm->is_object()) continue;

         const std::string uid = GetStr(*jm, "userId", "");
         if (uid.empty()) continue;

         st.TouchUser(uid);

         if (ut == "PRESENCE_PARTY_MEMBER_UPDATE_TYPE_ADD") {
            auto& mi = party.members[uid];
            mi.isOwner = BoolAt(*jm, "/isOwner"_json_pointer, false);
            mi.mmState = StrAt(*jm, "/state"_json_pointer, "");
            st.users[uid].online = true; // party ADD implies online enough
            changed = true;
            if (st.users[uid].nickname.empty()) { // nickname best-effort (staticData JSON string)
               std::string nick = ExtractNicknameFromStaticData(*jm);
               if (!nick.empty()) st.users[uid].nickname = std::move(nick);
            }
         } else if (ut == "PRESENCE_PARTY_MEMBER_UPDATE_TYPE_REMOVE") {
            changed |= (party.members.erase(uid) > 0);
            // we don't set online to any particular value on remove
         } else if (ut == "PRESENCE_PARTY_MEMBER_UPDATE_TYPE_UPDATE") {
            auto& mi = party.members[uid]; // create if missing
            mi.isOwner = BoolAt(*jm, "/isOwner"_json_pointer, mi.isOwner);
            const std::string s = StrAt(*jm, "/state"_json_pointer, "");
            if (!s.empty()) mi.mmState = s;
            st.users[uid].online = true; // party UPDATE implies online enough
            changed = true;
         } else {
            OutputDebugString((ut + " -- to handle as Party operation\n").c_str());
         }
      }
   }

   // Recompute leader
   changed |= party.RecomputeLeaderFromOwners();

   return changed;
}

bool StartServerController::HandlePartyInviteAcceptRequest(SdkPacket& u)
{
   const auto& p = u.payload;

   std::string uid = ExtractUserIdentity(p);
   if (uid.empty())
      return false;

   bool changed = false;

   // Touch user and mark online (accepting an invite implies "online enough")
   st.TouchUser(uid);
   if (!st.users[uid].online) {
      changed = true;
   }
   st.users[uid].online = true;

   // Accepting an invite implies leaving any current party.
   changed |= st.RemoveUserFromAllParties(uid);

   return changed;
}
bool StartServerController::HandlePartyDisbandRequest(SdkPacket& u)
{
   const auto& p = u.payload;
   const std::string uid = ExtractUserIdentity(p);
   if (uid.empty())
      return false;

   st.TouchUser(uid);
   st.users[uid].online = true;

   bool removedAny = false;

   // Disband is usually issued by the leader.
   // We remove any party where:
   //  - leaderUid matches, OR
   //  - member[uid].isOwner == true, OR
   //  - (fallback) the party contains uid and has <=1 member (useful if leader flag hasn't arrived yet)
   for (auto it = st.parties.begin(); it != st.parties.end(); ) {
      PartyState& party = it->second;

      bool isLeader = (!party.leaderUid.empty() && party.leaderUid == uid);

      auto itM = party.members.find(uid);
      bool memberOwner = (itM != party.members.end() && itM->second.isOwner);

      bool fallbackSolo = (itM != party.members.end() && party.members.size() <= 1);

      if (isLeader || memberOwner || fallbackSolo) {
         // erase whole party state; projector will stop rendering it
         it = st.parties.erase(it);
         removedAny = true;
         continue;
      }

      ++it;
   }

   return removedAny;
}

// dedicated-server handlers
bool StartServerController::HandleDedicatedServerHandshake(SdkPacket& u)
{
   const std::string serverId = u.payload.value("serverId", "");
   if (serverId.empty())
      return false; // HandshakeRequest can be empty

   st.TouchServer(serverId);
   auto& s = st.servers[serverId];

   bool changed = false;
   const bool modern = u.payload.value("isModernApi", false);
   if (s.isModernApi != modern) {
      s.isModernApi = modern;
      changed = true;
   }

   if (s.nodeKind != NodeKind::HeatedDSServer) {
      s.nodeKind = NodeKind::HeatedDSServer;
      changed = true;
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

bool StartServerController::HandleDedicatedServerSessionInfo(SdkPacket& u)
{
   const std::string serverId = u.payload.value("serverId", "");
   if (serverId.empty())
      return false;

   st.TouchServer(serverId);
   auto& s = st.servers[serverId];

   bool changed = false;
   const std::string refreshAfter = u.payload.value("refreshAfter", "");
   if (s.refreshAfter != refreshAfter) {
      s.refreshAfter = refreshAfter;
      changed = true;
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

static std::string JsonGetString(const nlohmann::json& j, std::initializer_list<const char*> path)
{
   const nlohmann::json* cur = &j;
   for (const char* key : path) {
      auto it = cur->find(key);
      if (it == cur->end()) return {};
      cur = &(*it);
   }
   return cur->is_string() ? cur->get<std::string>() : std::string{};
}

bool StartServerController::HandleGetStandaloneSignInCodeRequest(SdkPacket& u)
{
   const std::string uid = JsonGetString(u.payload, { "userContext", "data", "userIdentity" });
   if (uid.empty())
      return false;

   standaloneCorr.pendingGetCodeUserId = uid;
   return false; // correlation only, no graph change yet
}

bool StartServerController::HandleGetStandaloneSignInCodeResponse(SdkPacket& u)
{
   const std::string code = JsonGetString(u.payload, { "standaloneCode" });
   if (code.empty() || standaloneCorr.pendingGetCodeUserId.empty())
      return false;

   standaloneCorr.codeToHydraUserId[code] = standaloneCorr.pendingGetCodeUserId;
   standaloneCorr.pendingGetCodeUserId.clear();
   return false; // still correlation only
}

bool StartServerController::HandleSignInStandaloneCodeRequest(SdkPacket& u)
{
   const std::string code = JsonGetString(u.payload, { "standaloneCode" });
   if (code.empty())
      return false;

   auto it = standaloneCorr.codeToHydraUserId.find(code);
   standaloneCorr.pendingSignInUserId =
      (it != standaloneCorr.codeToHydraUserId.end()) ? it->second : std::string{};

   return false;
}

bool StartServerController::HandleSignInStandaloneCodeResponse(SdkPacket& u)
{
   const std::string serverToken = JsonGetString(u.payload, { "serverToken" });
   if (serverToken.empty() || standaloneCorr.pendingSignInUserId.empty())
      return false;

   standaloneCorr.serverTokenToHydraUserId[serverToken] = standaloneCorr.pendingSignInUserId;
   standaloneCorr.pendingSignInUserId.clear();
   return false;
}

bool StartServerController::HandleStandaloneHeartbeatServerRequest(SdkPacket& u)
{
   const std::string sid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (sid.empty())
      return false;

   st.TouchServer(sid);
   auto& s = st.servers[sid];

   bool changed = false;

   if (s.serverId != sid) {
      s.serverId = sid;
      changed = true;
   }

   if (s.nodeKind != NodeKind::StandaloneServer) {
      s.nodeKind = NodeKind::StandaloneServer;
      changed = true;
   }

   const std::string serverName = JsonGetString(u.payload, { "update", "data", "serverName", "data" });
   if (!serverName.empty() && s.serverName != serverName) {
      s.serverName = serverName;
      changed = true;
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

bool StartServerController::HandleStandaloneServerGetChallengesRequest(SdkPacket& u)
{
   const std::string sid = JsonGetString(u.payload, { "context", "data", "kernelSessionId" });
   if (sid.empty())
      return false;

   st.TouchServer(sid);
   auto& s = st.servers[sid];

   bool changed = false;

   if (s.serverId != sid) {
      s.serverId = sid;
      changed = true;
   }

   if (s.nodeKind != NodeKind::StandaloneServer) {
      s.nodeKind = NodeKind::StandaloneServer;
      changed = true;
   }

   auto it = u.payload.find("userIds");
   if (it != u.payload.end() && it->is_array() && !it->empty() && (*it)[0].is_string()) {
      const std::string uid = (*it)[0].get<std::string>();
      if (!uid.empty() && s.hydraUserId != uid) {
         s.hydraUserId = uid;
         changed = true;
      }
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

bool StartServerController::HandleCreateStandaloneServerRequest(SdkPacket& u)
{
   const std::string serverToken = JsonGetString(u.payload, { "serverToken" });
   if (serverToken.empty())
      return false;

   auto it = standaloneCorr.serverTokenToHydraUserId.find(serverToken);
   if (it == standaloneCorr.serverTokenToHydraUserId.end())
      return false;

   PendingStandaloneCreate ctx;
   ctx.hydraUserId = it->second;
   ctx.serverName = JsonGetString(u.payload, { "createData", "serverName", "data" });

   standaloneCorr.pendingCreates.push_back(std::move(ctx));
   return false;
}

bool StartServerController::HandleCreateStandaloneServerResponse(SdkPacket& u)
{
   const std::string sid = JsonGetString(u.payload, { "serverContext", "data", "kernelSessionId" });
   if (sid.empty())
      return false;

   st.TouchServer(sid);
   auto& s = st.servers[sid];

   bool changed = false;

   if (s.serverId != sid) {
      s.serverId = sid;
      changed = true;
   }

   if (s.nodeKind != NodeKind::StandaloneServer) {
      s.nodeKind = NodeKind::StandaloneServer;
      changed = true;
   }

   if (!standaloneCorr.pendingCreates.empty()) {
      PendingStandaloneCreate ctx = std::move(standaloneCorr.pendingCreates.front());
      standaloneCorr.pendingCreates.pop_front();

      if (s.hydraUserId != ctx.hydraUserId) {
         s.hydraUserId = ctx.hydraUserId;
         changed = true;
      }

      if (s.serverName != ctx.serverName) {
         s.serverName = ctx.serverName;
         changed = true;
      }
   }

   s.lastSeenTimeS = u.recv_time_s;
   return changed;
}

} // namespace Sample::UI::Controllers

