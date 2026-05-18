#include <string_view>
#include <algorithm>
#include <iterator>
#include <utility>

#include "ui/controllers/live_state.h"

#pragma message("live_state.cpp REV: SC sessions v0.3")

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

void LiveState::TouchServer(const std::string& sid)
{
   auto& s = servers[sid];
   if (s.serverId.empty()) {
      s.serverId = sid;
      serverOrder.push_back(sid);
   }
}

void LiveState::TouchSCSession(const std::string& scid)
{
   auto& s = scSessions[scid];
   if (s.scSessionId.empty()) {
      s.scSessionId = scid;
      scSessionOrder.push_back(scid);
   }
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
   }
   return false;
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

void LiveState::TouchSession(const std::string& sid)
{
   auto& s = sessions[sid];
   if (!s.seen) {
      s.seen = true;
      s.sessionId = sid;
      sessionOrder.push_back(sid);
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
   return removedAny;
}

} // namespace Sample::UI::Controllers
