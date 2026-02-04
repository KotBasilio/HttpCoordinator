#pragma once

#include <string>
#include <deque>
#include <cstdint>
#include <cstddef>

#include "ui/utils/graph_types.h"

#pragma message("main_model.h REV: Graph nodes v0.2")

namespace Sample::UI::Models {


enum class State {
   DEFAULT,
   USER_LOGGING,
   SERVER_STARTING,
   USER_LOGGED,
   SERVER_STARTED
};

enum class LogLevel : uint8_t {
   Info,
   Warn,
   Error
};

struct LogEntry {
   uint32_t    seq = 0;
   LogLevel    level = LogLevel::Info;
   double      time_s = 0.0;     // caller-provided timestamp (e.g. ImGui::GetTime())
   std::string text;
};

struct LogBuffer {
   std::deque<LogEntry> entries;
   uint32_t nextSeq = 1;
   size_t   maxEntries = 5000;

   void Push(LogLevel lvl, std::string msg, double time_s = 0.0);

   void Clear() { entries.clear(); }
};

struct MainModel {
   ~MainModel();

   State appState = State::DEFAULT;

   bool loginCheck = false;
   bool serverStartCheck = false;
   bool friendsCheck = false;
   bool logsCheck = false;
   bool messagesCheck = false;
   bool profileCheck = false;
   bool partyLobbyCheck = false;
   bool searchSessionCheck = false;
   bool serverSearchSessionCheck = false;
   bool sessionControlCheck = false;

   bool matchmakingCheck = false;
   bool qrFlowCheck = false;
   bool gameConfigCheck = false;
   bool hydraOssCheck = false;
   bool lyraCheck = false;
   bool graphUsesIngestion = false;

   std::string userName;
   LogBuffer logs;
   ::GraphModel graph;
};

} // namespace Sample::UI::Models
