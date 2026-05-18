# ImGui Coordinator — Active Task Context

Last updated: 2026-05-17

## Current focus

Continue the Coordinator graph evolution work.

The current active feature area is:

Party / Matchmaking / User Facts visualization from real Hydra / SDK event streams.

The tool should help us understand multiplayer/session behavior by turning noisy request/push logs into a stable visual graph.

Current graph story:

Server → SCSession → HydraSample → User → Party → MMSession

## Current baseline

The recent working baseline includes:

- Real HTTP ingestion into `SdkPacket`
- Dispatch by `SdkPacket::reqNameId`
- Reducers updating `LiveState`
- Projector rebuilding `GraphModel` from `LiveState`
- User key/value facts displayed in Inspector
- SCSession layer between Server and Hydra
- Server → SCSession link from dedicated-server session info
- SCSession → HydraSample link from verified SessionControl user context
- Dynamic sticky columns:
  - Hydra column: always
  - User column: always
  - Party column: appears once renderable Party exists, then stays for the run
  - MM column: appears once renderable MM session exists, then stays for the run
- Party nodes
- MMSession nodes
- Party → MM link reduction:
  - if Party member set matches MMSession member set
  - and MM leader belongs to that Party
  - replace direct User → MMSession links with Party → MMSession link

## Important architecture rule

Do not directly hand-edit graph nodes from packet handlers.

Use this flow:

1. HTTP request arrives.
2. Convert request into `SdkPacket`.
3. Dispatch by `SdkPacket::reqNameId`.
4. Reducer updates `LiveState`.
5. At the end of the ingestion drain batch:
   - if anything changed, call projector once.
6. Projector clears/rebuilds `GraphModel` deterministically.

This prevents dangling links and partial graph state.

## Files to inspect first

For ingestion / event handling:

- `start_server_controller.h`
- `packets_ingestion.cpp`
- `packet_json_helpers.h`
- `users_ingestion.cpp`
- `presence_ingestion.cpp`
- `servers_ingestion.cpp`

For graph layout / projection:

- `projector.cpp`
- `graph_types.h`

For selected-node UI:

- `inspector_panel.cpp`
- `inspector_panel.h`

For logs:

- `logs_panel.cpp`
- `main_model.h`

## Current design seams

### Identity seam

The SDK uses multiple identity-like values:

- `context.data.userIdentity`
- `member.userId`
- Facts `USER_ID`

Do not assume they are the same.

This mismatch is one of the reasons this Coordinator exists.

When adding logic that connects users, parties, and MM sessions, inspect actual payload paths first.

Possible future improvement:
- maintain an identity map from `userIdentity` to canonical `USER_ID` when Facts packets reveal both.

### SessionControl / SCSession binding

Current reliable evidence:

- `Hydra.Api.SessionControl.CreateSessionRequest`
  - stores pending user-side context:
    - `context.data.userIdentity`
    - `context.data.kernelSessionId`
    - or equivalent `userContext.data.*`
- `Hydra.Api.SessionControl.CreateSessionResponse`
  - uses `gameSessionId` as the SC session id
  - attaches the pending user-side context to that SC session
- `Hydra.Api.SessionControl.GetServerInfoRequest`
  - directly confirms `gameSessionId` plus `userContext.data.userIdentity` and user/client `kernelSessionId`
- `Hydra.Api.SessionControl.GetSessionEventsRequest`
  - stores pending SC id from `serverContext.data.kernelSessionId`
- `Hydra.Api.SessionControl.GetSessionEventsResponse`
  - attaches member contexts from `serverUserContext.userContext.data.*` to the pending SC id

Do not collapse `userIdentity`, Facts `USER_ID`, and user/client `kernelSessionId`. Store observed values and link only when the SessionControl packet flow proves the relationship.

### Party ↔ MMSession binding

Preferred current heuristic:

If an MM session has the same member set as a Party, and the MM leader belongs to that Party:
- remove direct User → MMSession links
- add Party → MMSession link
- keep the MMSession visible based on reducer state, not link count

Future stronger binding may use Facts packets that contain both:

- `PARTY_SESSION_ID`
- `MATCHMAKE_SESSION_ID`

### Empty Party / Session behavior

Current visual behavior:
- Hide empty Party nodes.
- Hide empty MMSession nodes.
- Keep Party/MM columns sticky once they were visible in the run.

SCSession nodes are state-backed and should only appear when reducers have observed a real SC id such as `gameSessionId` or `serverContext.data.kernelSessionId`.

## Near-term tasks

### Task 1 — Inspect current Party integration

Verify that Party reducer correctly handles:

- `Hydra.Api.Push.Presence.PresencePartyUpdate`
- party creation
- member add/remove/update
- leader change
- join code
- party disband

Check that Party nodes appear/disappear as expected.

### Task 2 — Inspect dynamic layout

Verify:

- Party column appears only after a renderable Party exists.
- MM column appears only after a renderable MM session exists.
- Columns stay sticky after appearing.
- Party and MM Y positions are based on average member User Y.
- Overlap nudging is stable.

### Task 3 — Improve Party / MM binding only if evidence supports it

Before changing Party ↔ MM binding, inspect actual Facts and Presence packets.

Do not guess new semantics if the stream does not prove them.

### Task 4 — Validate SCSession linking

Verify against real SessionControl traffic:

- CreateSessionRequest followed by CreateSessionResponse creates SCSession → HydraSample.
- GetServerInfoRequest confirms/updates the same link.
- GetSessionEventsRequest/Response can add member Hydra contexts to the SCSession.
- Dedicated-server session info still creates Server → SCSession.
- Inspector shows factual SC keys such as `SC_GAME_SESSION_ID`, `SC_SERVER_KERNEL_SESSION_ID`, `HYDRA_USER_IDENTITY`, and `HYDRA_KERNEL_SESSION_ID`.

## Validation approach

Recommended manual validation:

1. Run app.
2. Feed known Hydra logs / replay stream.
3. Watch graph evolution:
   - Server/SCSession nodes appear when real server/session-control events arrive.
   - SCSession links to HydraSample when SessionControl context proves the relation.
   - HydraSample/User nodes appear.
   - User facts appear in Inspector.
   - Party node appears when Party exists.
   - MM node appears when matchmaking exists.
   - Party → MM reduction happens when member sets match.
4. Select User nodes and verify key/value facts.
5. Select Party/MM nodes and verify title/subtitle/links.

If tests exist, run them. If no tests exist, prefer a small reproducible replay/log scenario over broad changes.

## Coding style reminders

- Prefer small, reviewable patches.
- Preserve reducer/projector separation.
- Prefer stable, readable C++ over cleverness.
- Use existing helper style (`NodeAt`, `StrAt`, `BoolAt`, etc.) if present.
- Avoid broad refactors unless explicitly requested.
- If adding a new packet handler, dispatch via `SdkPacket::reqNameId`.
- If adding a new request name, update the request-name generation pipeline rather than string-prefix hacks.

## Collaboration protocol

Before editing, do a brief sync:

1. List relevant files inspected.
2. List their `#pragma message("... REV: ...")` stamps if present.
3. Summarize current understanding.
4. Propose the smallest patch.
5. Then edit.

Project ritual names:

- `↔️G-sync`: confirm current files/revisions.
- `⛓️G-rev`: review synced code.
- `🔏G-lock`: accepted baseline.

Codex may commit and push in this repository when explicitly asked. Small commits are preferred because the human owner reviews Codex changes through Git.

Workflow context:
- The full app is compiled and run on Win11 in the production Visual Studio project.
- This WSL repository is a small Codex working subset. It may omit `.vcxproj` files and some UI/source files from the full project.
- The human owner bridges code between this subset and the Win11 project with local scripts.
- Codex commits here are intended to make review/history easy; the human owner handles production integration after review.

## Do not do yet

- Do not implement a full historical replay UI unless asked.
- Do not redesign the whole layout system.
- Do not collapse User/Party/MM identity into one key without evidence.
- Do not remove diagnostic logging that helps understand SDK events.
