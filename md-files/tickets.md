# ImGui Coordinator - Tickets

Small Jira-style task list for valid work that should stay visible without crowding `active.md`.

Keep this list light. Current task direction still belongs in `active.md`; accepted background belongs in `history.md`.

| ID | Status | Area | Summary | Notes |
|---|---|---|---|---|
| UI-001 | Blocked | Inspector | Copy icon polish | Replace the kv row `[  ]` copy placeholder with the intended copy icon after the icon artists provide the asset. Preserve current copy behavior, tooltip, and table layout. |
| SC-001 | Deferred | Server / SCSession | Harden activation correlation for interleaved streams | Current pending Server -> SCSession activation state is acceptable for observed ordered logs. Promote this when logs show interleaved server/session activation responses or multi-server ambiguity. |
