#include "projector_internal.h"

#include <algorithm>
#include <unordered_set>

namespace Sample::UI::Controllers
{

static std::string FindSessionLeaderUid(const SessionState& s)
{
   std::string leader;
   for (const auto& mkv : s.members) {
      if (mkv.second.isOwner) {
         leader = mkv.first;
         return leader;
      }
   }

   // no owner
   return {};
}

bool SessionState::SameMemberSet(const PartyState::MapT& other) const
{
   if (members.size() != other.size()) return false;
   for (const auto& kv : members) {
      if (other.find(kv.first) == other.end()) return false;
   }
   return true;
}

static void RemoveUserLinksToSession(GraphModel& g, NodeId sessNodeId,
   const std::unordered_set<NodeId>& userNodeIds)
{
   auto& links = g.links;
   links.erase(std::remove_if(links.begin(), links.end(),
      [&](const GraphLink& L) {
         return (L.to == sessNodeId) && (userNodeIds.find(L.from) != userNodeIds.end());
      }), links.end());
}

void StartServerController::ReduceLinksPartyOwnsSession()
{
   // If a party explicitly names an MM session, or perfectly "owns" a session
   // by member set + leader, hide the user->session links and add a party->session
   // link for cleaner visualization.
   auto& graph = mainModel->graph;

   // For each MM session, find a matching party by:
   // - explicit Party attr match
   // - fallback: leader is in party and member sets equal
   for (const std::string& sid : st.sessionOrder) {
      auto itS = st.sessions.find(sid);
      if (itS == st.sessions.end()) continue;
      const SessionState& sess = itS->second;
      if (sess.members.empty()) continue;

      const PartyState* bestParty = nullptr;

      // Strong binding: Party data attrs name the concrete MM session id.
      for (const std::string& pid : st.partyOrder) {
         auto itP = st.parties.find(pid);
         if (itP == st.parties.end()) continue;
         const PartyState& party = itP->second;
         if (party.members.empty()) continue;

         if (party.matchmakingSessionId == sid) {
            bestParty = &party;
            break;
         }
      }

      // Fallback heuristic: leader is in party AND member sets match.
      const std::string leaderUid = FindSessionLeaderUid(sess);
      if (!bestParty && !leaderUid.empty()) {
         for (const std::string& pid : st.partyOrder) {
            if (bestParty) break;

            auto itP = st.parties.find(pid);
            if (itP == st.parties.end()) continue;
            const PartyState& party = itP->second;
            if (party.members.empty()) continue;

            if (party.members.find(leaderUid) == party.members.end())
               continue;

            if (!sess.SameMemberSet(party.members))
               continue;

            bestParty = &party;
            break;
         }
      }

      if (!bestParty) continue;

      const NodeId sessNodeId = HashToNodeId(sid, salt.mm);
      const NodeId partyNodeId = HashToNodeId(bestParty->partyId, salt.party);

      // Drop user->session links for these members
      std::unordered_set<NodeId> userNodeIds;
      userNodeIds.reserve(sess.members.size());
      for (const auto& mkv : sess.members) {
         const NodeId userNodeId = HashToNodeId(mkv.first, salt.user);
         userNodeIds.insert(userNodeId);
      }
      RemoveUserLinksToSession(graph, sessNodeId, userNodeIds);

      // add Party->Session link
      EnsureLink(graph, partyNodeId, sessNodeId);

      if (GraphNode* partyNode = FindNode(graph, partyNodeId))
         AddKvIfMissing(partyNode->kv, "REDUCED_MM_SESSION_ID", sid);
      if (GraphNode* sessNode = FindNode(graph, sessNodeId))
         AddKvIfMissing(sessNode->kv, "REDUCED_BY_PARTY_ID", bestParty->partyId);
   }
}

} // namespace Sample::UI::Controllers
