// live_state.h
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <deque>

#include "ui/utils/graph_types.h"

namespace Sample::UI::Controllers {

struct UserState
{
   std::string userId;      // userIdentity/userId
   std::string userIdentity;
   std::string hydraKernelSessionId; // client/user-side kernelSessionId
   std::string runtimeSeanceId;
   std::string nickname;    // best effort
   std::string platform;
   std::string providerId;
   std::string userIdentityType;
   bool online = false;
   bool isLocal = true; // placeholder until Forge wires concrete local-user evidence

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
   std::string reason;
   std::string joinCode; // best effort, may be empty
   std::string leaderUid; // computed from members' isOwner
   std::string gameSessionId;
   std::string matchmakingSessionId;
   std::string partyMaxCount;
   std::string joinDelegation;
   std::string joinable;
   std::string disbandOnOwnerLeave;
   std::string longOperationCorrelationId;
   std::string longOperationUserId;

   // members by userId
   struct MemberInfo {
      bool isOwner = false;
      std::string mmState; // MATCHMAKE_STATE_QUEUE/GAME if present
      std::string nickname;
      std::string provider;
      std::string buildPlatform;
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
   std::string reason;
   std::string mmState; // "MATCHMAKE_STATE_QUEUE" / "MATCHMAKE_STATE_GAME" / etc.
   std::string playlistId;
   std::string dataCenterId;
   std::string sessionType;
   std::string jip;
   std::string maxPlayers;
   std::string joinDelegation;
   std::string longOperationCorrelationId;
   std::string longOperationUserId;
   std::vector<std::pair<std::string, std::string>> variants;

   // members by userId
   struct MemberInfo {
      std::string groupId;
      bool isOwner = false;
      std::string rating;
      std::string sortingIndex;
      std::string memberState;
      std::string classRole;
      std::string nickname;
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
   bool hasSessionInfo = false;
   std::string dsApiVersion;
   std::string serverVersion;
   std::string reportedIp;
   std::string reportedIpv6;
   std::string authEndpointService;
   std::string authEndpointIp;
   std::string authEndpointPort;
   std::string connectionInfo;
   std::string serverProperty;
   std::string serverState;
   double lastSeenTimeS = 0.0;

   int viewIdx = -1;
   float canvasY = 0.0f;

   std::string serverName;            // standalone server name
   std::string hydraUserId;           // attach to HydraSample keyed by uid
   std::string scSessionId;           // linked SessionControl session id, when proven
};

struct SCSessionState {
   std::string scSessionId;

   // Optional relationships for future reducers.
   std::string serverId;
   std::string hydraUserId;
   std::string hydraKernelSessionId;
   std::string serverContextKernelSessionId;
   std::string dataCenterId;
   std::string clientVersion;
   std::string serverData;
   std::string connectionInfo;
   std::string serverProperty;
   std::string acceptStatus;
   std::string refreshAfterSeconds;
   std::string lastSessionMemberEventId;
   std::string serverFactSessionId;
   std::unordered_map<std::string, std::string> hydraUsers;

   int viewIdx = -1;
   float canvasY = 0.0f;
};

struct PendingSessionControlCreate {
   std::string hydraUserId;
   std::string hydraKernelSessionId;
   std::string dataCenterId;
   std::string clientVersion;
   std::string serverData;
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

   PendingSessionControlCreate pendingSCCreate;
   std::string pendingGetSessionEventsSCSessionId;
   std::string pendingGetServerInfoSCSessionId;
   std::string pendingGetServerSessionInfoServerId;
   std::string pendingSCActivationServerId;
   std::deque<std::string> pendingFinishSessionSCSessionIds;
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
   bool TouchSession(const std::string& sid);
   void TouchParty(const std::string& pid);
   bool TouchServer(const std::string& sid);
   bool TouchSCSession(const std::string& scid);
   bool RemoveSession(const std::string& sid);
   bool RemoveServer(const std::string& sid);
   bool RemoveSCSession(const std::string& scid);

   void Tombstone(const std::string& entityKey);
   bool IsTombstoned(std::string_view entityKey) const;
   void ClearTombstone(std::string_view entityKey);

   void ResetOwners();
   void RefreshOwnerFlagForUser(const std::string& uid);
   void UnbindUser(const std::string& uid);
   bool RemoveUserFromAllParties(const std::string& uid);
   bool MoveEntityInOrder(NodeKind kind, std::string_view entityKey, int delta);

   float CalcYForNode(SessionState& s) const;
   float CalcYForNode(PartyState& p)   const;
private:
   template <class ItemT, class OrderT, class TableT>
   float YforGroupAsCentroid(ItemT& item, const OrderT& order, const TableT& table) const;

   // graveyard of removed entities, to avoid re-adding them in the same run
   std::unordered_set<std::string> tombstonedEntityKeys;
};

} // namespace Sample::UI::Controllers
