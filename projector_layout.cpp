#include "projector_internal.h"

#include <cmath>

#pragma message("projector_layout.cpp REV: projector split v0.1")

namespace Sample::UI::Controllers
{

template<class ItemT, class OrderT, class TableT>
float LiveState::YforGroupAsCentroid(ItemT& item, const OrderT& order, const TableT& table) const
{
   // Step 1: centroid of members' user canvasY
   // users are projected already (see ProjectHydraUsers)
   float sumY = 0.0f;
   int   cnt = 0;
   for (const auto& mkv : item.members) {
      const std::string& uid = mkv.first;
      auto itU = users.find(uid);
      if (itU == users.end()) continue;
      sumY += itU->second.canvasY;
      ++cnt;
   }

   float y;
   if (cnt > 0) {
      y = sumY / (float)cnt;
   } else {
      const int idx = (item.viewIdx >= 0) ? item.viewIdx : 0;
      y = lay.yBase + (float)idx * lay.yStep;
   }

   // Step 2: overlap resolution (nudge down below earlier items)
   bool adjusted;
   do {
      adjusted = false;

      for (const auto& key : order) {
         auto it = table.find(key);
         if (it == table.end()) continue;

         const auto& other = it->second;
         if (other.viewIdx < 0) continue;
         if (other.viewIdx >= item.viewIdx) continue;

         if (std::fabs(y - other.canvasY) < lay.minGap) {
            y = other.canvasY + lay.yStep;
            adjusted = true;
         }
      }
   } while (adjusted);

   return y;
}

float LiveState::CalcYForNode(SessionState& s) const { return YforGroupAsCentroid(s, sessionOrder, sessions); }
float LiveState::CalcYForNode(PartyState& p)   const { return YforGroupAsCentroid(p, partyOrder, parties); }

void StartServerController::UpdateColumnsInLayout()
{
   // Sticky column enablement: once visible, never turns off this run
   bool partyVisibleNow = false;
   for (const auto& pkv : st.parties)
      if (!pkv.second.members.empty()) { partyVisibleNow = true; break; }
   if (partyVisibleNow) st.partyEverVisible = true;

   bool mmVisibleNow = false;
   for (const auto& skv : st.sessions)
      if (!skv.second.members.empty()) { mmVisibleNow = true; break; }
   if (mmVisibleNow) st.mmEverVisible = true;

   bool serversVisibleNow = false;
   for (const auto& dskv : st.servers)
      if (!dskv.second.serverId.empty()) { serversVisibleNow = true; break; }

   // Compute X positions from active columns
   float x = lay.xUser;
   x += lay.xStep;
   if (st.partyEverVisible) { lay.xParty = x; x += lay.xStep; }
   if (st.mmEverVisible) { lay.xMM = x; x += lay.xStep; }

   (void)serversVisibleNow; // intentional for now: semantic visibility, no layout shift yet
   // possible: if (serversVisibleNow) { lay.xServer = x; x += lay.xStep; }
}

} // namespace Sample::UI::Controllers
