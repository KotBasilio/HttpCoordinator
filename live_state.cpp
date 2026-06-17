#include <string_view>
#include <algorithm>
#include <iterator>
#include <utility>

#include "ui/controllers/live_state.h"

namespace Sample::UI::Controllers {

static bool StartsWith(std::string_view s, std::string_view pre)
{
   return s.size() >= pre.size() && s.compare(0, pre.size(), pre) == 0;
}

static std::string_view StripPrefix(std::string_view s, std::string_view pre)
{
   return StartsWith(s, pre) ? s.substr(pre.size()) : s;
}

static bool MoveAdjacent(std::vector<std::string>& order, std::string_view key, int delta)
{
   if (delta == 0 || order.size() < 2) return false;

   auto it = std::find(order.begin(), order.end(), key);
   if (it == order.end()) return false;

   const size_t idx = (size_t)std::distance(order.begin(), it);

   if (delta < 0) {
      if (idx == 0) return false; // no-op
      std::swap(order[idx], order[idx - 1]);
      return true;
   }

   if (idx + 1 >= order.size()) return false; // no-op

   std::swap(order[idx], order[idx + 1]);
   return true;
}

static bool RemoveFromOrder(std::vector<std::string>& order, const std::string& key)
{
   auto it = std::find(order.begin(), order.end(), key);
   if (it == order.end())
      return false;

   order.erase(it);
   return true;
}

static std::string ServerEntityKey(const std::string& sid)
{
   return "server:" + sid;
}

static std::string SCSessionEntityKey(const std::string& scid)
{
   return "scsession:" + scid;
}

static std::string MMSessionEntityKey(const std::string& sid)
{
   return "mmsession:" + sid;
}

bool LiveState::TouchServer(const std::string& sid)
{
   if (IsTombstoned(ServerEntityKey(sid)))
      return false;

   auto& s = servers[sid];
   if (s.serverId.empty()) {
      s.serverId = sid;
      serverOrder.push_back(sid);
   }

   return true;
}

bool LiveState::TouchSession(const std::string& sid)
{
   if (IsTombstoned(MMSessionEntityKey(sid)))
      return false;

   auto& s = sessions[sid];
   if (!s.seen) {
      s.seen = true;
      s.sessionId = sid;
      sessionOrder.push_back(sid);
   }

   return true;
}

bool LiveState::TouchSCSession(const std::string& scid)
{
   if (IsTombstoned(SCSessionEntityKey(scid)))
      return false;

   auto& s = scSessions[scid];
   if (s.scSessionId.empty()) {
      s.scSessionId = scid;
      scSessionOrder.push_back(scid);
   }

   return true;
}

bool LiveState::RemoveSession(const std::string& sid)
{
   tombstonedEntityKeys.insert(MMSessionEntityKey(sid));
   const size_t erased = sessions.erase(sid);
   const bool removedOrder = RemoveFromOrder(sessionOrder, sid);
   return erased > 0 || removedOrder;
}

bool LiveState::RemoveServer(const std::string& sid)
{
   tombstonedEntityKeys.insert(ServerEntityKey(sid));
   const size_t erased = servers.erase(sid);
   const bool removedOrder = RemoveFromOrder(serverOrder, sid);
   return erased > 0 || removedOrder;
}

bool LiveState::RemoveSCSession(const std::string& scid)
{
   tombstonedEntityKeys.insert(SCSessionEntityKey(scid));
   const size_t erased = scSessions.erase(scid);
   const bool removedOrder = RemoveFromOrder(scSessionOrder, scid);
   return erased > 0 || removedOrder;
}

bool LiveState::IsTombstoned(std::string_view entityKey) const
{
   return tombstonedEntityKeys.find(std::string(entityKey)) != tombstonedEntityKeys.end();
}

void LiveState::ClearTombstone(std::string_view entityKey)
{
   tombstonedEntityKeys.erase(std::string(entityKey));
}

bool LiveState::MoveEntityInOrder(NodeKind kind, std::string_view entityKey, int delta)
{
   // GraphNode.entityKey has prefixes in projector.cpp: "user:", "party:", "mmsession:", etc.
   // Our order vectors store raw ids (uid/pid/sid), so strip prefix here.
   switch (kind) {
      case NodeKind::User:
         return MoveAdjacent(userOrder, StripPrefix(entityKey, "user:"), delta);
      case NodeKind::Party:
         return MoveAdjacent(partyOrder, StripPrefix(entityKey, "party:"), delta);
      case NodeKind::MMSession:
         return MoveAdjacent(sessionOrder, StripPrefix(entityKey, "mmsession:"), delta);
      case NodeKind::SCSession:
         return MoveAdjacent(scSessionOrder, StripPrefix(entityKey, "scsession:"), delta);
      case NodeKind::HeatedDSServer:
      case NodeKind::StandaloneServer:
         return MoveAdjacent(serverOrder, StripPrefix(entityKey, "server:"), delta);
      default:
         return false;
   }
}
   
void LiveState::TouchUser(const std::string& uid)
{
   auto& u = users[uid];
   if (!u.seen) {
      u.seen = true;
      u.userId = uid;
      userOrder.push_back(uid);
   }
}

void LiveState::TouchParty(const std::string& pid)
{
   auto& p = parties[pid];
   if (!p.seen) {
      p.seen = true;
      p.partyId = pid;
      partyOrder.push_back(pid);
   }
}

void LiveState::ResetOwners()
{
   // Reset owner in users
   for (auto& kv : users) {
      kv.second.isOwnerAny = false;
   }

   // Recalc owner in sessions
   // reset viewIdx as well, we'll recalc it in ProjectToGraph
   for (auto& skv : sessions) {
      skv.second.viewIdx = -1;
      for (auto& mkv : skv.second.members) {
         if (mkv.second.isOwner) {
            auto itU = users.find(mkv.first);
            if (itU != users.end()) {
               itU->second.isOwnerAny = true;
            }
         }
      }
   }

   // Recalc owner/leader from parties.
   for (auto& pkv : parties) {
      pkv.second.viewIdx = -1;
      const std::string& leaderUid = pkv.second.leaderUid;
      if (leaderUid.empty())
         continue;

      auto itU = users.find(leaderUid);
      if (itU != users.end()) {
         itU->second.isOwnerAny = true;
      }
   }
}

void LiveState::RefreshOwnerFlagForUser(const std::string& uid)
{
   auto itU = users.find(uid);
   if (itU == users.end())
      return;

   bool isOwner = false;

   for (const auto& skv : sessions) {
      auto itM = skv.second.members.find(uid);
      if (itM != skv.second.members.end() && itM->second.isOwner) {
         isOwner = true;
         break;
      }
   }

   if (!isOwner) {
      for (const auto& pkv : parties) {
         if (pkv.second.leaderUid == uid) {
            isOwner = true;
            break;
         }
      }
   }

   itU->second.isOwnerAny = isOwner;
}

void LiveState::UnbindUser(const std::string& uid)
{
   // Remove user from all sessions 
   for (auto& skv : sessions) {
      skv.second.members.erase(uid);
   }

   // Remove user from all parties
   RemoveUserFromAllParties(uid);
}

// Recompute leader from members (first owner wins; stable enough for v1)
bool PartyState::RecomputeLeaderFromOwners()
{
   std::string leader;
   for (const auto& mkv : members) {
      if (mkv.second.isOwner) { leader = mkv.first; break; }
   }
   if (leader == leaderUid) return false;
   leaderUid = std::move(leader);
   return true;
}

// Remove member, keep leader consistent.
// Returns true if the member was removed.
bool PartyState::RemoveMember(const std::string& uid)
{
   if (members.erase(uid) == 0) return false;
   RecomputeLeaderFromOwners(); // safe even if no owners remain
   return true;
}

bool LiveState::RemoveUserFromAllParties(const std::string& uid)
{
   bool removedAny = false;
   for (auto it = parties.begin(); it != parties.end(); ) {
      auto& party = it->second;
      if (party.RemoveMember(uid)) {
         removedAny = true;
         if (party.members.empty()) {
            it = parties.erase(it);
            continue;
         }
      }
      ++it;
   }

   if (removedAny)
      RefreshOwnerFlagForUser(uid);

   return removedAny;
}

} // namespace Sample::UI::Controllers
