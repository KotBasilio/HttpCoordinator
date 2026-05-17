// live_state.h
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <utility>
#include <deque>

#include "ui/utils/graph_types.h"

namespace Sample::UI::Controllers {

struct UserState
{
   std::string userId;      // userIdentity/userId
   std::string nickname;    // best effort
   bool online = false;

   // propertyName/propertyValue
   std::vector<std::pair<std::string, std::string>> facts; 

   // derived flags for display
   bool isOwnerAny = false;

   // view
   bool  seen = false;
   float canvasY = 0.0f;
};

struct PartyState {
   std::string partyId;
   std::string joinCode; // best effort, may be empty
   std::string leaderUid; // computed from members' isOwner

   // members by userId
   struct MemberInfo {
      bool isOwner = false;
      std::string mmState; // MATCHMAKE_STATE_QUEUE/GAME if present
   };
   typedef std::unordered_map<std::string, MemberInfo> MapT;
   MapT members;

   // view
   bool  seen = false;
   int   viewIdx = -1;
   float canvasY = 0.0f;

   bool RecomputeLeaderFromOwners();
   bool RemoveMember(const std::string& uid);
};

struct SessionState
{
   std::string sessionId;
   std::string mmState; // "MATCHMAKE_STATE_QUEUE" / "MATCHMAKE_STATE_GAME" / etc.

   // members by userId
   struct MemberInfo {
      std::string groupId;
      bool isOwner = false;
   };
   typedef std::unordered_map<std::string, MemberInfo> MapT;
   MapT members;

   // view
   bool seen = false;
   int viewIdx = -1;
   float canvasY = 0.0f;

   bool SameMemberSet(const PartyState::MapT& other) const;
};

struct ServerState {
   std::string serverId;              // standalone: kernelSessionId
   NodeKind nodeKind = NodeKind::HeatedDSServer;

   bool isModernApi = false;
   std::string refreshAfter;
   double lastSeenTimeS = 0.0;

   int viewIdx = -1;
   float canvasY = 0.0f;

   std::string serverName;            // standalone server name
   std::string hydraUserId;           // attach to HydraSample keyed by uid
};

struct SCSessionState {
   std::string scSessionId;

   // Optional relationships for future reducers.
   std::string serverId;
   std::string hydraUserId;

   int viewIdx = -1;
   float canvasY = 0.0f;
};

struct PendingStandaloneCreate {
   std::string hydraUserId;
   std::string serverName;
};

struct StandaloneCorrelationState {
   std::string pendingGetCodeUserId;
   std::string pendingSignInUserId;

   std::unordered_map<std::string, std::string> codeToHydraUserId;
   std::unordered_map<std::string, std::string> serverTokenToHydraUserId;

   std::deque<PendingStandaloneCreate> pendingCreates;
};

struct LiveState
{
   // users + visual ordering (first-seen)
   std::unordered_map<std::string, UserState> users;
   std::vector<std::string> userOrder;     

   // sessions + visual ordering (first-seen)
   std::unordered_map<std::string, SessionState> sessions;
   std::vector<std::string> sessionOrder;

   // parties + visual ordering (first-seen)
   std::unordered_map<std::string, PartyState> parties;
   std::vector<std::string> partyOrder;

   // sticky columns: once visible in this run, never turn off
   bool partyEverVisible = false;
   bool mmEverVisible = false;

   // servers
   std::unordered_map<std::string, ServerState> servers;
   std::vector<std::string> serverOrder;

   // session-control sessions
   std::unordered_map<std::string, SCSessionState> scSessions;
   std::vector<std::string> scSessionOrder;

   void TouchUser(const std::string& uid);
   void TouchSession(const std::string& sid);
   void TouchParty(const std::string& pid);
   void TouchServer(const std::string& sid);
   void TouchSCSession(const std::string& scid);

   void ResetOwners();
   void UnbindUser(const std::string& uid);
   bool RemoveUserFromAllParties(const std::string& uid);
   bool MoveEntityInOrder(NodeKind kind, std::string_view entityKey, int delta);

   float CalcYForNode(SessionState& s) const;
   float CalcYForNode(PartyState& p)   const;
private:
   template <class ItemT, class OrderT, class TableT>
   float YforGroupAsCentroid(ItemT& item, const OrderT& order, const TableT& table) const;
};

} // namespace Sample::UI::Controllers
