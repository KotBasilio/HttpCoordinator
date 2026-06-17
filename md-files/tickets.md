# ImGui Coordinator - Tickets

Small Jira-style task list for valid work that should stay visible without crowding `active.md`.

Keep this list light. Current task direction still belongs in `active.md`; accepted background belongs in `history.md`.

| ID | Status | Area | Summary | Notes |
|---|---|---|---|---|
| SC-001 | Deferred | Server / SCSession | Harden activation correlation for interleaved streams | Current pending Server -> SCSession activation state is acceptable for observed ordered logs. Promote this when logs show interleaved server/session activation responses or multi-server ambiguity. |
| RED-001 | Deferred | Reducer correlation | Replace single-slot pending request/response state when interleaving appears | Several reducer correlations keep one pending request context at a time: standalone sign-in code, SC create, GetServerInfo, GetSessionEvents, DS session info, and activation. This is acceptable for observed ordered streams, but interleaved traffic could attach a response to the wrong request. Promote when logs show concurrent flows or ambiguous pairings; use queues/maps keyed by concrete request evidence rather than graph-side fixes. |
| ID-001 | Deferred | Identity | Map Hydra identity seams when evidence appears | The SDK exposes `context.data.userIdentity`, `member.userId`, and Facts `USER_ID`. Do not collapse them without packet evidence; Facts may later support an explicit identity map. |
| HYDRA-001 | Deferred | Hydra identity | Merge local users through local Hydra instance state | `task_ctx/log_3_users.analysis.md` shows three local Hydra connect flows but only one current `RUNTIME_SEANCE_ID`. Add minimal local-Hydra-instance state so one proven runtime seance can key the shared HydraSample while per-user fallback remains for unproven users. |
| HYDRA-002 | Deferred | Facts / Hydra identity | Guard against stale runtime seance facts | In `log_3_users.txt`, one Facts packet carries an older header `KERNEL_SESSION_ID` / `factSessionId` under the current user context. Prefer runtime-seance identity evidence whose Facts header kernel matches the packet user context; keep mismatches as debug evidence or ignore for identity. |
| HYDRA-003 | Deferred | Test / replay | Add regression coverage for Hydra local-user merge | Build or use a replay/test path for the three-local-users scenario: locally observed users should share one HydraSample when runtime evidence appears, but remote MM members should not merge solely because they share an MM session. |
| MM-001 | Review | Party / MMSession | Strengthen Party to MM binding | `BigRunTest_02.txt` showed Party attrs `MatchmakingSessionID_s` / `GameSessionID_s` as stronger evidence than the member-set heuristic. Projector now prefers explicit Party -> MMSession binding and keeps the heuristic as fallback; pending Archy runtime review. |
| UI-002 | Deferred | Inspector | Consider property sections beyond `GraphNode::kv` | Keep `GraphNode::kv` until structured property sections solve real pressure rather than theoretical neatness. |
| UI-003 | Deferred | Graph | Historical replay UI | Do not build replay/history browsing until the workflow explicitly needs it. |
| UI-004 | Deferred | Layout | Global layout redesign | Preserve current column and centroid layout unless a promoted task calls for a broader redesign. |
