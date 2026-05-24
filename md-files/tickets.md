# ImGui Coordinator - Tickets

Small Jira-style task list for valid work that should stay visible without crowding `active.md`.

Keep this list light. Current task direction still belongs in `active.md`; accepted background belongs in `history.md`.

| ID | Status | Area | Summary | Notes |
|---|---|---|---|---|
| UI-001 | Done | Inspector visuals | Copy icon polish | Inspector kv rows use the generated `IC_COPY_16PX` icon with the existing copy behavior and tooltip. |
| SC-001 | Deferred | Server / SCSession | Harden activation correlation for interleaved streams | Current pending Server -> SCSession activation state is acceptable for observed ordered logs. Promote this when logs show interleaved server/session activation responses or multi-server ambiguity. |
| ID-001 | Deferred | Identity | Map Hydra identity seams when evidence appears | The SDK exposes `context.data.userIdentity`, `member.userId`, and Facts `USER_ID`. Do not collapse them without packet evidence; Facts may later support an explicit identity map. |
| MM-001 | Deferred | Party / MMSession | Strengthen Party to MM binding | Current Party -> MMSession reduction uses a member-set heuristic. Promote a stronger binding if Facts expose both `PARTY_SESSION_ID` and `MATCHMAKE_SESSION_ID`. |
| UI-002 | Deferred | Inspector | Consider property sections beyond `GraphNode::kv` | Keep `GraphNode::kv` until structured property sections solve real pressure rather than theoretical neatness. |
| UI-003 | Deferred | Graph | Historical replay UI | Do not build replay/history browsing until the workflow explicitly needs it. |
| UI-004 | Deferred | Layout | Global layout redesign | Preserve current column and centroid layout unless a promoted task calls for a broader redesign. |
