#include "ui/controllers/start_server_controller.h"
#include "ui/controllers/coordinator_http_server.h"
#include "packet_json_helpers.h"

#include <algorithm>
#include <utility>
#include <vector>

#pragma message("presence_ingestion.cpp REV: rich kv v0.1")

namespace Sample::UI::Controllers
{

using namespace nlohmann::literals; // enables "... "_json_pointer

static bool SetIfDifferent(std::string& dst, const std::string& src)
{
   if (src.empty() || dst == src)
      return false;

   dst = src;
   return true;
}

static std::string StringFromJsonValue(const Json& value)
{
   if (value.is_string()) return value.get<std::string>();
   if (value.is_number_integer()) return std::to_string(value.get<long long>());
   if (value.is_number_unsigned()) return std::to_string(value.get<unsigned long long>());
   if (value.is_number_float()) return std::to_string(value.get<double>());
   if (value.is_boolean()) return value.get<bool>() ? "true" : "false";
   return {};
}

static std::string ExtractStaticDataValue(const Json& memberData, const char* key)
{
   const std::string sd = GetStr(memberData, "staticData", "");
   if (sd.size() < 2 || sd.front() != '{')
      return {};

   const auto j = Json::parse(sd, nullptr, false);
   if (j.is_discarded())
      return {};

   return JsonGetString(j, { key });
}

static bool ApplySessionVariants(SessionState& sess, const Json& variants)
{
   if (!variants.is_array())
      return false;

   std::vector<std::pair<std::string, std::string>> next;
   for (const auto& v : variants) {
      const std::string id = JsonGetString(v, { "id" });
      if (id.empty())
         continue;

      std::string value = JsonGetString(v, { "value" });
      if (value.empty()) {
         if (auto it = v.find("values"); it != v.end() && it->is_array() && !it->empty()) {
            value = StringFromJsonValue((*it)[0]);
         }
      }

      if (!value.empty())
         next.push_back({ id, value });
   }

   std::sort(next.begin(), next.end(), [](const auto& a, const auto& b) {
      return a.first < b.first;
   });

   if (sess.variants == next)
      return false;

   sess.variants = std::move(next);
   return true;
}

bool StartServerController::HandleMatchmakeSessionLeaveRequest(SdkPacket& u)
{
   const auto& p = u.payload;

   // Request contains userIdentity in context.data.userIdentity (as in your sample).
   const std::string uid = ExtractUserIdentity(p);
   if (uid.empty())
      return false;

   // Ensure user exists in state; leaving MM != going offline.
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

// PresenceSessionUpdate is the truth stream for MM state + membership.
bool StartServerController::HandleMMSessionUpdate(SdkPacket& u)
{
   const auto& p = u.payload;

   // get to session id
   const std::string sessionId = ExtractSessionId(p);
   if (sessionId.empty()) {
      return false;
   }
   st.TouchSession(sessionId);
   auto& sess = st.sessions[sessionId];
   bool changed = false;

   changed |= SetIfDifferent(sess.reason, JsonGetString(p, { "id", "reason" }));
   changed |= SetIfDifferent(sess.mmState, GetStr(p, "state", ""));
   changed |= SetIfDifferent(sess.playlistId, JsonGetString(p, { "queueData", "playListId" }));
   changed |= SetIfDifferent(sess.playlistId, JsonGetString(p, { "gameData", "playListId", "data" }));
   changed |= SetIfDifferent(sess.dataCenterId, JsonGetString(p, { "gameData", "dataCenterId" }));
   changed |= SetIfDifferent(sess.sessionType, JsonGetString(p, { "gameData", "sessionType" }));
   changed |= SetIfDifferent(sess.jip, JsonGetString(p, { "gameData", "settings", "jip" }));
   changed |= SetIfDifferent(sess.maxPlayers, JsonGetString(p, { "gameData", "settings", "maxPlayers" }));
   changed |= SetIfDifferent(sess.joinDelegation, JsonGetString(p, { "gameData", "settings", "joinDelegation" }));
   changed |= SetIfDifferent(sess.longOperationCorrelationId, JsonGetString(p, { "longOperationStatus", "correlationId" }));
   changed |= SetIfDifferent(sess.longOperationUserId, JsonGetString(p, { "longOperationStatus", "userId" }));

   if (const Json* variants = NodeAt(p, "/variants"_json_pointer); variants && variants->is_array()) {
      changed |= ApplySessionVariants(sess, *variants);
   }
   if (const Json* queueData = NodeAt(p, "/queueData"_json_pointer); queueData && queueData->is_object()) {
      changed |= ApplySessionVariants(sess, queueData->value("queueVariants", Json::array()));
   }
   if (const Json* gameData = NodeAt(p, "/gameData"_json_pointer); gameData && gameData->is_object()) {
      changed |= ApplySessionVariants(sess, gameData->value("variants", Json::array()));
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
         info.rating = JsonGetString(md, { "rating" });
         info.sortingIndex = JsonGetString(md, { "sortingIndex" });
         info.memberState = JsonGetString(md, { "state" });
         info.classRole = JsonGetString(md, { "classRole" });
         info.nickname = ExtractNicknameFromStaticData(md);
         sess.members[uid] = std::move(info);
         usr.online = true;
      } else if (op == "PRESENCE_SESSION_MEMBER_UPDATE_TYPE_REMOVE") {
         sess.members.erase(uid);
      } else if (op == "PRESENCE_SESSION_MEMBER_UPDATE_TYPE_UPDATE") {
         auto& info = sess.members[uid]; // create if missing
         SetIfDifferent(info.groupId, StrAt(md, "/groupId"_json_pointer, ""));
         info.isOwner = BoolAt(md, "/isOwner"_json_pointer, info.isOwner);
         SetIfDifferent(info.rating, JsonGetString(md, { "rating" }));
         SetIfDifferent(info.sortingIndex, JsonGetString(md, { "sortingIndex" }));
         SetIfDifferent(info.memberState, JsonGetString(md, { "state" }));
         SetIfDifferent(info.classRole, JsonGetString(md, { "classRole" }));
         SetIfDifferent(info.nickname, ExtractNicknameFromStaticData(md));
      } else {
         OutputDebugString((op + " -- to handle as MMSessionMembers operation\n").c_str());
      }
   }

   return changed;
}

bool StartServerController::HandlePartyUpdate(SdkPacket& u)
{
   const auto& p = u.payload;

   // Party id is normally here.
   static const auto PID = "/id/id"_json_pointer;
   const Json* jPid = NodeAt(p, PID);
   const std::string partyId = (jPid && jPid->is_string()) ? jPid->get<std::string>() : "";

   if (partyId.empty()) {
      // Some updates are reason-only; skip for now (we can map later if needed).
      return false;
   }

   st.TouchParty(partyId);
   auto& party = st.parties[partyId];

   bool changed = false;
   changed |= SetIfDifferent(party.reason, JsonGetString(p, { "id", "reason" }));
   changed |= SetIfDifferent(party.partyMaxCount, JsonGetString(p, { "settings", "partyMaxCount" }));
   changed |= SetIfDifferent(party.joinDelegation, JsonGetString(p, { "settings", "joinDelegation" }));
   changed |= SetIfDifferent(party.joinable, JsonGetString(p, { "settings", "joinable" }));
   changed |= SetIfDifferent(party.disbandOnOwnerLeave, JsonGetString(p, { "settings", "disbandOnOwnerLeave" }));
   changed |= SetIfDifferent(party.longOperationCorrelationId, JsonGetString(p, { "longOperationStatus", "correlationId" }));
   changed |= SetIfDifferent(party.longOperationUserId, JsonGetString(p, { "longOperationStatus", "userId" }));

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
   const Json* jMu = NodeAt(p, MU);
   if (!jMu) {
      static const auto MU1 = "/members"_json_pointer;
      jMu = NodeAt(p, MU1);
   }

   if (jMu && jMu->is_array()) {
      for (const auto& e : *jMu) {
         if (!e.is_object()) continue;

         const std::string ut = GetStr(e, "updateType", "");
         const Json* jm = NodeAt(e, "/member"_json_pointer);
         if (!jm || !jm->is_object()) continue;

         const std::string uid = GetStr(*jm, "userId", "");
         if (uid.empty()) continue;

         st.TouchUser(uid);

         if (ut == "PRESENCE_PARTY_MEMBER_UPDATE_TYPE_ADD") {
            auto& mi = party.members[uid];
            mi.isOwner = BoolAt(*jm, "/isOwner"_json_pointer, false);
            mi.mmState = StrAt(*jm, "/state"_json_pointer, "");
            SetIfDifferent(mi.nickname, ExtractNicknameFromStaticData(*jm));
            SetIfDifferent(mi.provider, ExtractStaticDataValue(*jm, "provider"));
            SetIfDifferent(mi.buildPlatform, JsonGetString(*jm, { "platformData", "buildPlatform" }));
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
            SetIfDifferent(mi.nickname, ExtractNicknameFromStaticData(*jm));
            SetIfDifferent(mi.provider, ExtractStaticDataValue(*jm, "provider"));
            SetIfDifferent(mi.buildPlatform, JsonGetString(*jm, { "platformData", "buildPlatform" }));
            st.users[uid].online = true; // party UPDATE implies online enough
            changed = true;
         } else {
            OutputDebugString((ut + " -- to handle as Party operation\n").c_str());
         }
      }
   }

   // Recompute leader.
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

   // Touch user and mark online (accepting an invite implies "online enough").
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

} // namespace Sample::UI::Controllers
