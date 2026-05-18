#pragma once

#include "ui/controllers/start_server_controller.h"

#include <string>
#include <utility>
#include <vector>

namespace Sample::UI::Controllers
{

struct LayoutConfig
{
   // Columns defaults
   static constexpr float xBase = 80.0f;
   static constexpr float xHydra = xBase;
   static constexpr float xStep = 180.0f;
   static constexpr float xUser = xHydra + xStep;
   static constexpr float xSCSession = xHydra - xStep;
   static constexpr float xServer = xSCSession - xStep;

   // Rows
   static constexpr float yBase = 80.0f;
   static constexpr float yStep = 80.0f;
   static constexpr float minGap = 60.0f;

   // Columns dynamic defaults (can be updated based on visibility of columns)
   float xParty = xUser + xStep;
   float xMM = xParty + xStep;

   // sizes
   Vec2f s32{32, 32};
};

struct SaltVariants
{
   uint32_t hydra = 0x48445241u; // "HDRA"
   uint32_t user = 0x55534552u;  // "USER"
   uint32_t mm = 0x4D4D5345u;    // "MMSE"
   uint32_t party = 0x50415254u; // "PART"
   uint32_t server = 0x44535256u; // "DSRV"
   uint32_t sc = 0x53435345u;    // "SCSE"
};

extern LayoutConfig lay;
extern SaltVariants salt;

GraphNode& UpsertNode(GraphModel& g, NodeId id);
void EnsureLink(GraphModel& g, NodeId from, NodeId to,
   GraphPort fromPort = GraphPort::Right, GraphPort toPort = GraphPort::Left);
NodeId HashToNodeId(const std::string& s, uint32_t salt);
GraphNode* FindNode(GraphModel& g, NodeId id);

void AddKv(std::vector<std::pair<std::string, std::string>>& kv,
   const char* key,
   const std::string& value);
void AddKv(std::vector<std::pair<std::string, std::string>>& kv,
   const char* key,
   bool value);
void AddKvIfMissing(std::vector<std::pair<std::string, std::string>>& kv,
   const char* key,
   const std::string& value,
   const char* existingKey = nullptr);

void FillServerKv(GraphNode& n, const ServerState& s, const std::string& serverId);
void FillSCSessionKv(GraphNode& n, const SCSessionState& s, const std::string& scid,
   const std::vector<std::string>& hydraUserIds);
void FillHydraKv(GraphNode& n, const UserState& u);
void FillUserKv(GraphNode& n, const UserState& u);
void FillMMSessionKv(GraphNode& n, const SessionState& s, const std::string& sid);
void FillPartyKv(GraphNode& n, const PartyState& p, const std::string& pid);

} // namespace Sample::UI::Controllers
