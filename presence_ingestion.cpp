#include "ui/controllers/start_server_controller.h"
#include "ui/controllers/coordinator_http_server.h"
#include "packet_json_helpers.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace Sample::UI::Controllers
{

using namespace nlohmann::literals; // enables "... "_json_pointer

// Centralized updateType and reasons definitions
static constexpr const char* PRESENCE_SESSION_MEMBER_UPDATE_TYPE_ADD    = "PRESENCE_SESSION_MEMBER_UPDATE_TYPE_ADD";
static constexpr const char* PRESENCE_SESSION_MEMBER_UPDATE_TYPE_REMOVE = "PRESENCE_SESSION_MEMBER_UPDATE_TYPE_REMOVE";
static constexpr const char* PRESENCE_SESSION_MEMBER_UPDATE_TYPE_UPDATE = "PRESENCE_SESSION_MEMBER_UPDATE_TYPE_UPDATE";

static constexpr const char* GAME_SESSION_ID_CHANGE_REASON_JOIN = "GAME_SESSION_ID_CHANGE_REASON_JOIN";

static constexpr const char* PARTY_CHANGE_REASON_LEAVE = "PARTY_ID_CHANGE_REASON_LEAVE";

static constexpr const char* PRESENCE_PARTY_MEMBER_UPDATE_TYPE_ADD    = "PRESENCE_PARTY_MEMBER_UPDATE_TYPE_ADD";
static constexpr const char* PRESENCE_PARTY_MEMBER_UPDATE_TYPE_REMOVE = "PRESENCE_PARTY_MEMBER_UPDATE_TYPE_REMOVE";
static constexpr const char* PRESENCE_PARTY_MEMBER_UPDATE_TYPE_UPDATE = "PRESENCE_PARTY_MEMBER_UPDATE_TYPE_UPDATE";

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

static std::string ExtractPartyAttrValue(const Json& p, const char* key)
{
   const std::string dataBlob = JsonGetString(p, { "data", "data" });
   if (dataBlob.size() < 2 || dataBlob.front() != '{')
      return {};

   const auto j = Json::parse(dataBlob, nullptr, false);
   if (j.is_discarded())
      return {};

   return JsonGetString(j, { "Attrs", key });
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

static bool SameMMSessionMemberInfo(const SessionState::MemberInfo& a,
   const SessionState::MemberInfo& b)
{
   return a.groupId == b.groupId
      && a.isOwner == b.isOwner
      && a.rating == b.rating
      && a.sortingIndex == b.sortingIndex
      && a.memberState == b.memberState
      && a.classRole == b.classRole
      && a.nickname == b.nickname
      && a.provider == b.provider
      && a.extendedData == b.extendedData;
}

static bool SameMMSessionMembers(const SessionState::MapT& a,
   const SessionState::MapT& b)
{
   if (a.size() != b.size())
      return false;

   for (const auto& kv : a) {
      auto it = b.find(kv.first);
      if (it == b.end())
         return false;

      if (!SameMMSessionMemberInfo(kv.second, it->second))
         return false;
   }

   return true;
}

static SessionState::MemberInfo BuildMMSessionMemberInfo(const Json& memberData)
{
   SessionState::MemberInfo info{};
   info.groupId = GetStr(memberData, "groupId", "");
   info.isOwner = BoolAt(memberData, "/isOwner"_json_pointer, false);
   info.rating = JsonGetString(memberData, { "rating" });
   info.sortingIndex = JsonGetString(memberData, { "sortingIndex" });
   info.memberState = JsonGetString(memberData, { "state" });
   info.classRole = JsonGetString(memberData, { "classRole" });
   info.nickname = ExtractNicknameFromStaticData(memberData);
   info.provider = ExtractStaticDataValue(memberData, "provider");
   info.extendedData = ExtractStaticDataValue(memberData, "extendedData");
   return info;
}

static bool ApplyMMSessionMemberSnapshot(LiveState& st, SessionState& sess, const Json& members)
{
   if (!members.is_array())
      return false;

   bool changed = false;
   SessionState::MapT nextMembers;

   for (const auto& member : members) {
      if (!member.is_object())
         continue;

      const std::string uid = GetStr(member, "userId", "");
      if (uid.empty())
         continue;

      st.TouchUser(uid);
      auto& usr = st.users[uid];
      usr.online = true;

      const std::string nick = ExtractNicknameFromStaticData(member);
      if (!nick.empty() && usr.nickname != nick) {
         usr.nickname = nick;
         changed = true;
      }

      nextMembers[uid] = BuildMMSessionMemberInfo(member);
   }

   if (!SameMMSessionMembers(sess.members, nextMembers)) {
      sess.members = std::move(nextMembers);
      changed = true;
   }

   return changed;
}

static bool HasMMSessionMemberAdd(const Json& p)
{
   const Json* gameData = NodeAt(p, Json::json_pointer("/gameData"));
   if (!gameData || !gameData->is_object())
      return false;

   const Json* membersUpdate = NodeAt(*gameData, Json::json_pointer("/membersUpdate"));
   if (!membersUpdate || !membersUpdate->is_array())
      return false;

   for (const auto& update : *membersUpdate) {
      if (!update.is_object())
         continue;

      if (GetStr(update, "updateType", "") == PRESENCE_SESSION_MEMBER_UPDATE_TYPE_ADD)
         return true;
   }

   return false;
}

static bool IsMMSessionLifecycleStart(const Json& p)
{
   if (JsonGetString(p, { "id", "reason" }) == GAME_SESSION_ID_CHANGE_REASON_JOIN)
      return true;

   return HasMMSessionMemberAdd(p);
}

static bool IsUserOwnerInAnyActiveGroup(const LiveState& st, const std::string& uid)
{
   for (const auto& skv : st.sessions) {
      const auto itMember = skv.second.members.find(uid);
      if (itMember != skv.second.members.end() && itMember->second.isOwner)
         return true;
   }

   for (const auto& pkv : st.parties) {
      const PartyState& party = pkv.second;
      if (party.leaderUid == uid)
         return true;

      const auto itMember = party.members.find(uid);
      if (itMember != party.members.end() && itMember->second.isOwner)
         return true;
   }

   return false;
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
   std::vector<std::string> emptySessionIds;
   for (auto& skv : st.sessions) {
      auto& sess = skv.second;
      if (sess.members.erase(uid) > 0) {
         removedAny = true;
         if (sess.members.empty())
            emptySessionIds.push_back(skv.first);
      }
   }

   for (const std::string& sid : emptySessionIds)
      removedAny |= st.RemoveSession(sid);

   return removedAny;
}

bool StartServerController::HandleMatchmakeSessionRemoveMembersRequest(SdkPacket& u)
{
   const auto& p = u.payload;
   const std::string actorUid = ExtractUserIdentity(p);
   auto itActor = st.users.find(actorUid);
   if (actorUid.empty() || itActor == st.users.end() || !IsUserOwnerInAnyActiveGroup(st, actorUid)) {
      if (mainModel) {
         const std::string msg = "HandleMatchmakeSessionRemoveMembersRequest: actor is not owner: "
            + (actorUid.empty() ? std::string("<empty>") : actorUid);
         mainModel->logs.Error(msg);
      }
      return false;
   }

   auto itUserIds = p.find("userId");
   if (itUserIds == p.end() || !itUserIds->is_array() || itUserIds->empty())
      return false;

   std::vector<std::string> userIds;
   userIds.reserve(itUserIds->size());
   for (const auto& item : *itUserIds) {
      if (!item.is_string())
         continue;

      const std::string uid = item.get<std::string>();
      if (uid.empty())
         continue;

      st.TouchUser(uid);
      st.users[uid].online = true;
      userIds.push_back(uid);
   }

   if (userIds.empty())
      return false;

   bool changed = false;
   std::vector<std::string> emptySessionIds;

   for (auto& skv : st.sessions) {
      auto& sess = skv.second;
      bool removedHere = false;
      for (const std::string& uid : userIds) {
         if (sess.members.erase(uid) > 0) {
            removedHere = true;
            changed = true;
         }
      }

      if (removedHere && sess.members.empty())
         emptySessionIds.push_back(skv.first);
   }

   for (const std::string& sid : emptySessionIds) {
      changed |= st.RemoveSession(sid);
   }

   return changed;
}

bool StartServerController::HandleMatchmakeSessionGetInfoRequest(SdkPacket& u)
{
   const std::string sessionId = GetStr(u.payload, "sessionId", "");
   if (sessionId.empty())
      return false;

   standaloneCorr.pendingMMGetInfoSessionIds.push_back(sessionId);
   return false;
}

bool StartServerController::HandleMatchmakeSessionGetInfoResponse(SdkPacket& u)
{
   const auto& p = u.payload;
   const Json* sessionData = NodeAt(p, "/sessionData"_json_pointer);
   std::string sessionId = sessionData ? JsonGetString(*sessionData, { "id" }) : std::string{};

   if (!standaloneCorr.pendingMMGetInfoSessionIds.empty()) {
      if (sessionId.empty())
         sessionId = standaloneCorr.pendingMMGetInfoSessionIds.front();

      standaloneCorr.pendingMMGetInfoSessionIds.pop_front();
   }

   if (sessionId.empty())
      return false;

   const std::string tombstoneKey = "mmsession:" + sessionId;
   if (st.IsTombstoned(tombstoneKey))
      return false;

   if (!st.TouchSession(sessionId))
      return false;

   auto& sess = st.sessions[sessionId];
   bool changed = false;

   changed |= SetIfDifferent(sess.isJoinable, JsonGetString(p, { "isJoinable" }));

   if (!sessionData || !sessionData->is_object())
      return changed;

   changed |= SetIfDifferent(sess.dataCenterId, JsonGetString(*sessionData, { "dataCenterId" }));
   changed |= SetIfDifferent(sess.jip, JsonGetString(*sessionData, { "settings", "jip" }));
   changed |= SetIfDifferent(sess.maxPlayers, JsonGetString(*sessionData, { "settings", "maxPlayers" }));
   changed |= SetIfDifferent(sess.joinDelegation, JsonGetString(*sessionData, { "settings", "joinDelegation" }));
   changed |= ApplySessionVariants(sess, sessionData->value("variants", Json::array()));

   if (const Json* members = NodeAt(*sessionData, "/sessionMembers"_json_pointer); members && members->is_array())
      changed |= ApplyMMSessionMemberSnapshot(st, sess, *members);

   return changed;
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
   const std::string tombstoneKey = "mmsession:" + sessionId;
   if (st.IsTombstoned(tombstoneKey)) {
      if (!IsMMSessionLifecycleStart(p))
         return false;

      st.ClearTombstone(tombstoneKey);
   }

   if (!st.TouchSession(sessionId))
      return false;
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

   bool removedMember = false;

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
      if (op == PRESENCE_SESSION_MEMBER_UPDATE_TYPE_ADD) {
         sess.members[uid] = BuildMMSessionMemberInfo(md);
         usr.online = true;
      } else if (op == PRESENCE_SESSION_MEMBER_UPDATE_TYPE_REMOVE) {
         if (sess.members.erase(uid) > 0) {
            removedMember = true;
         }
      } else if (op == PRESENCE_SESSION_MEMBER_UPDATE_TYPE_UPDATE) {
         auto& info = sess.members[uid]; // create if missing
         SetIfDifferent(info.groupId, StrAt(md, "/groupId"_json_pointer, ""));
         info.isOwner = BoolAt(md, "/isOwner"_json_pointer, info.isOwner);
         SetIfDifferent(info.rating, JsonGetString(md, { "rating" }));
         SetIfDifferent(info.sortingIndex, JsonGetString(md, { "sortingIndex" }));
         SetIfDifferent(info.memberState, JsonGetString(md, { "state" }));
         SetIfDifferent(info.classRole, JsonGetString(md, { "classRole" }));
         SetIfDifferent(info.nickname, ExtractNicknameFromStaticData(md));
         SetIfDifferent(info.provider, ExtractStaticDataValue(md, "provider"));
         SetIfDifferent(info.extendedData, ExtractStaticDataValue(md, "extendedData"));
      } else {
         OutputDebugString((op + " -- to handle as MMSessionMembers operation\n").c_str());
      }
   }

   if (removedMember && sess.members.empty()) {
      const std::string sessionId = sess.sessionId;
      changed |= st.RemoveSession(sessionId);
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
   changed |= SetIfDifferent(party.gameSessionId, ExtractPartyAttrValue(p, "GameSessionID_s"));
   changed |= SetIfDifferent(party.matchmakingSessionId, ExtractPartyAttrValue(p, "MatchmakingSessionID_s"));
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

   return HandlePartyDelta(u, party, changed);
}

bool StartServerController::HandlePartyDelta(SdkPacket& u, PartyState& party, bool changed)
{
   // Members update stream (delta)
   const auto& p = u.payload;
   static const auto MU = "/membersUpdate"_json_pointer;
   const Json* jMu = NodeAt(p, MU);
   if (!jMu) {
      static const auto MU1 = "/members"_json_pointer;
      jMu = NodeAt(p, MU1);
   }

   std::vector<std::string> ownerRefreshUsers;

   if (!jMu && party.reason == PARTY_CHANGE_REASON_LEAVE && party.members.size() <= 1) {
      for (const auto& mkv : party.members)
         ownerRefreshUsers.push_back(mkv.first);
      changed |= !party.members.empty();
      party.members.clear();
   } else if (jMu && jMu->is_array()) {
      for (const auto& e : *jMu) {
         if (!e.is_object()) continue;

         const std::string ut = GetStr(e, "updateType", "");
         const Json* jm = NodeAt(e, "/member"_json_pointer);
         if (!jm || !jm->is_object()) continue;

         const std::string uid = GetStr(*jm, "userId", "");
         if (uid.empty()) continue;

         st.TouchUser(uid);

         if (ut == PRESENCE_PARTY_MEMBER_UPDATE_TYPE_ADD) {
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
         } else if (ut == PRESENCE_PARTY_MEMBER_UPDATE_TYPE_REMOVE) {
            if (party.members.erase(uid) > 0) {
               ownerRefreshUsers.push_back(uid);
               changed = true;
            }
            // we don't set online to any particular value on remove
         } else if (ut == PRESENCE_PARTY_MEMBER_UPDATE_TYPE_UPDATE) {
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

   for (const std::string& uid : ownerRefreshUsers)
      st.RefreshOwnerFlagForUser(uid);

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
