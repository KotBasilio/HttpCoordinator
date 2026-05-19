#include "projector_internal.h"

#include <algorithm>

#pragma message("projector_keys_values.cpp REV: hydra identity v0.1")

namespace Sample::UI::Controllers
{

void AddKv(std::vector<std::pair<std::string, std::string>>& kv,
   const char* key,
   const std::string& value)
{
   if (!value.empty())
      kv.push_back({ key, value });
}

void AddKv(std::vector<std::pair<std::string, std::string>>& kv,
   const char* key,
   bool value)
{
   kv.push_back({ key, value ? "true" : "false" });
}

static std::string FindKvValue(const std::vector<std::pair<std::string, std::string>>& kv,
   const char* key)
{
   const auto it = std::find_if(kv.begin(), kv.end(),
      [key](const auto& row) { return row.first == key; });

   return (it == kv.end()) ? std::string{} : it->second;
}

void AddKvIfMissing(std::vector<std::pair<std::string, std::string>>& kv,
   const char* key,
   const std::string& value,
   const char* existingKey)
{
   if (value.empty())
      return;

   // Check if the key already exists
   const auto it = std::find_if(kv.begin(), kv.end(),
      [key](const auto& row) { return row.first == key; });
   if (it != kv.end())
      return;

   // Check if the value exists under the alternative key
   if (existingKey) {
      std::string existingValue = FindKvValue(kv, existingKey);
      if (existingValue == value)
         return;
   }

   kv.push_back({ key, value });
}

void FillServerKv(GraphNode& n, const ServerState& s, const std::string& serverId)
{
   n.kv.clear();
   AddKv(n.kv, "SERVER_ID", serverId);
   AddKv(n.kv, "SERVER_KIND", n.title);
   AddKv(n.kv, "IS_MODERN_API", s.isModernApi);
   AddKv(n.kv, "DS_API_VERSION", s.dsApiVersion);
   AddKv(n.kv, "SERVER_VERSION", s.serverVersion);
   //AddKv(n.kv, "REFRESH_AFTER", s.refreshAfter); too transient, don't add
   AddKv(n.kv, "HAS_SESSION_INFO", s.hasSessionInfo);
   AddKv(n.kv, "REPORTED_IP", s.reportedIp);
   AddKv(n.kv, "REPORTED_IPV6", s.reportedIpv6);
   AddKv(n.kv, "AUTH_ENDPOINT_SERVICE", s.authEndpointService);
   AddKv(n.kv, "AUTH_ENDPOINT_IP", s.authEndpointIp);
   AddKv(n.kv, "AUTH_ENDPOINT_PORT", s.authEndpointPort);
   AddKv(n.kv, "LINKED_SC_SESSION_ID", s.scSessionId);
   AddKv(n.kv, "CONNECTION_INFO", s.connectionInfo);
   AddKv(n.kv, "SERVER_PROPERTY", s.serverProperty);
   AddKv(n.kv, "SERVER_STATE", s.serverState);
}

void FillSCSessionKv(GraphNode& n, const SCSessionState& s, const std::string& scid,
   const std::vector<std::string>& hydraUserIds)
{
   n.kv.clear();
   AddKv(n.kv, "SC_SESSION_ID", scid);
   AddKv(n.kv, "GAME_SESSION_ID", scid);
   AddKv(n.kv, "SERVER_CONTEXT_KERNEL_SESSION_ID", s.serverContextKernelSessionId);
   AddKv(n.kv, "DATA_CENTER_ID", s.dataCenterId);
   AddKv(n.kv, "CLIENT_VERSION", s.clientVersion);
   AddKv(n.kv, "SERVER_DATA", s.serverData);
   AddKv(n.kv, "LINKED_SERVER_ID", s.serverId);
   AddKv(n.kv, "HYDRA_USER_IDENTITY", s.hydraUserId);
   AddKv(n.kv, "HYDRA_KERNEL_SESSION_ID", s.hydraKernelSessionId);
   AddKv(n.kv, "CONNECTION_INFO", s.connectionInfo);
   AddKv(n.kv, "SERVER_PROPERTY", s.serverProperty);
   AddKv(n.kv, "ACCEPT_STATUS", s.acceptStatus);
   //AddKv(n.kv, "REFRESH_AFTER_SECONDS", s.refreshAfterSeconds); too transient, don't add
   AddKv(n.kv, "LAST_SESSION_MEMBER_EVENT_ID", s.lastSessionMemberEventId);
   AddKv(n.kv, "SERVER_FACT_SESSION_ID", s.serverFactSessionId);
   if (!s.hydraUsers.empty())
      n.kv.push_back({ "HYDRA_MEMBER_COUNT", std::to_string(s.hydraUsers.size()) });

   for (const std::string& hydraUserId : hydraUserIds) {
      n.kv.push_back({ "HYDRA_MEMBER_USER_ID", hydraUserId });
   }
}

void FillHydraKv(GraphNode& n, const UserState& u, const std::string& hydraIdentityKey)
{
   n.kv.clear();
   AddKv(n.kv, "HYDRA_ENTITY_ID", hydraIdentityKey);
   AddKv(n.kv, "RUNTIME_SEANCE_ID", u.runtimeSeanceId);
   AddKv(n.kv, "HYDRA_USER_IDENTITY", u.userIdentity);
   AddKv(n.kv, "HYDRA_KERNEL_SESSION_ID", u.hydraKernelSessionId);
   AddKv(n.kv, "LINKED_USER_ID", u.userId);
   AddKv(n.kv, "PLATFORM", u.platform);
   AddKv(n.kv, "PROVIDER_ID", u.providerId);
   AddKv(n.kv, "USER_IDENTITY_TYPE", u.userIdentityType);
   AddKv(n.kv, "USER_ONLINE", u.online);
}

void FillUserKv(GraphNode& n, const UserState& u)
{
   n.kv = u.facts;
   AddKvIfMissing(n.kv, "USER_IDENTITY", u.userIdentity, "USER_ID");
   AddKvIfMissing(n.kv, "USER_ID", u.userId);
   AddKvIfMissing(n.kv, "ACCOUNT_NAME", u.nickname);
   AddKvIfMissing(n.kv, "PLATFORM", u.platform);
   AddKvIfMissing(n.kv, "PROVIDER_ID", u.providerId, "PROVIDER");
   AddKvIfMissing(n.kv, "USER_IDENTITY_TYPE", u.userIdentityType);
   AddKvIfMissing(n.kv, "KERNEL_SESSION_ID", u.hydraKernelSessionId);
   std::sort(n.kv.begin(), n.kv.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
}

void FillMMSessionKv(GraphNode& n, const SessionState& s, const std::string& sid)
{
   n.kv.clear();
   AddKv(n.kv, "MM_SESSION_ID", sid);
   AddKv(n.kv, "MM_REASON", s.reason);
   AddKv(n.kv, "MM_STATE", s.mmState);
   AddKv(n.kv, "MEMBER_COUNT", std::to_string(s.members.size()));
   AddKv(n.kv, "PLAYLIST_ID", s.playlistId);
   AddKv(n.kv, "DATA_CENTER_ID", s.dataCenterId);
   AddKv(n.kv, "SESSION_TYPE", s.sessionType);
   AddKv(n.kv, "JIP", s.jip);
   AddKv(n.kv, "MAX_PLAYERS", s.maxPlayers);
   AddKv(n.kv, "JOIN_DELEGATION", s.joinDelegation);
   AddKv(n.kv, "LONG_OPERATION_CORRELATION_ID", s.longOperationCorrelationId);
   AddKv(n.kv, "LONG_OPERATION_USER_ID", s.longOperationUserId);

   for (const auto& variant : s.variants) {
      AddKv(n.kv, ("VARIANT_" + variant.first).c_str(), variant.second);
   }

   std::vector<std::string> memberIds;
   memberIds.reserve(s.members.size());
   for (const auto& mkv : s.members)
      memberIds.push_back(mkv.first);
   std::sort(memberIds.begin(), memberIds.end());

   int memberIndex = 1;
   for (const std::string& uid : memberIds) {
      const auto& mi = s.members.at(uid);
      const std::string prefix = "MEMBER_" + std::to_string(memberIndex) + "_";
      AddKv(n.kv, (prefix + "USER_ID").c_str(), uid);
      AddKv(n.kv, (prefix + "NICKNAME").c_str(), mi.nickname);
      AddKv(n.kv, (prefix + "IS_OWNER").c_str(), mi.isOwner);
      AddKv(n.kv, (prefix + "GROUP_ID").c_str(), mi.groupId);
      AddKv(n.kv, (prefix + "RATING").c_str(), mi.rating);
      AddKv(n.kv, (prefix + "SORTING_INDEX").c_str(), mi.sortingIndex);
      AddKv(n.kv, (prefix + "MEMBER_STATE").c_str(), mi.memberState);
      AddKv(n.kv, (prefix + "CLASS_ROLE").c_str(), mi.classRole);
      ++memberIndex;
   }
}

void FillPartyKv(GraphNode& n, const PartyState& p, const std::string& pid)
{
   n.kv.clear();
   AddKv(n.kv, "PARTY_ID", pid);
   AddKv(n.kv, "PARTY_REASON", p.reason);
   AddKv(n.kv, "MEMBER_COUNT", std::to_string(p.members.size()));
   AddKv(n.kv, "LEADER_USER_ID", p.leaderUid);
   AddKv(n.kv, "JOIN_CODE", p.joinCode);
   AddKv(n.kv, "PARTY_MAX_COUNT", p.partyMaxCount);
   AddKv(n.kv, "JOIN_DELEGATION", p.joinDelegation);
   AddKv(n.kv, "JOINABLE", p.joinable);
   AddKv(n.kv, "DISBAND_ON_OWNER_LEAVE", p.disbandOnOwnerLeave);
   AddKv(n.kv, "LONG_OPERATION_CORRELATION_ID", p.longOperationCorrelationId);
   AddKv(n.kv, "LONG_OPERATION_USER_ID", p.longOperationUserId);

   std::vector<std::string> memberIds;
   memberIds.reserve(p.members.size());
   for (const auto& mkv : p.members)
      memberIds.push_back(mkv.first);
   std::sort(memberIds.begin(), memberIds.end());

   int memberIndex = 1;
   for (const std::string& uid : memberIds) {
      const auto& mi = p.members.at(uid);
      const std::string prefix = "MEMBER_" + std::to_string(memberIndex) + "_";
      AddKv(n.kv, (prefix + "USER_ID").c_str(), uid);
      AddKv(n.kv, (prefix + "NICKNAME").c_str(), mi.nickname);
      AddKv(n.kv, (prefix + "IS_OWNER").c_str(), mi.isOwner);
      AddKv(n.kv, (prefix + "MM_STATE").c_str(), mi.mmState);
      AddKv(n.kv, (prefix + "PROVIDER").c_str(), mi.provider);
      AddKv(n.kv, (prefix + "BUILD_PLATFORM").c_str(), mi.buildPlatform);
      ++memberIndex;
   }
}

} // namespace Sample::UI::Controllers
