# ImGui Coordinator — Development History

This file keeps accepted or background project knowledge that no longer needs to dominate `active.md`.

## Validated Architecture

### Ingestion and reducer/projector flow

Validated baseline:

1. HTTP request arrives.
2. Request becomes `SdkPacket`.
3. Dispatch happens by `SdkPacket::reqNameId`.
4. Reducers update `LiveState`.
5. At the end of the ingestion drain batch, if anything changed, the projector runs once.
6. Projector clears/rebuilds `GraphModel` deterministically.

Do not directly hand-edit graph nodes from packet handlers.

### Split ingestion files

`packets_ingestion.cpp` was split into smaller domain files:

- `packets_ingestion.cpp`: drain/dispatch anchor.
- `packet_json_helpers.cpp/.h`: JSON extraction helpers.
- `users_ingestion.cpp`: sign-in/sign-out and facts.
- `presence_ingestion.cpp`: Party/MM reducers.
- `servers_ingestion.cpp`: server, standalone server, and SessionControl reducers.

### Split projector files

`projector.cpp` was split into focused modules:

- `projector.cpp`: core projection orchestration.
- `projector_internal.h`: shared projector internals.
- `projector_keys_values.cpp`: `GraphNode::kv` population.
- `projector_layout.cpp`: columns and Y placement.
- `projector_nodes.cpp`: node creation/projection.
- `projector_links.cpp`: link creation and reduction.

The split is organizational only; the reducer → `LiveState` → projector → `GraphModel` architecture remains unchanged.

## Validated Graph Behavior

### User facts in Inspector

Validated behavior:
- `Pros.Api.Facts.WriteBinaryPackUserRequest` carries useful `propertyName` / `propertyValue` pairs.
- These are stored in reducer state and projected into `GraphNode::kv`.
- Inspector displays `GraphNode::kv` for selected nodes.
- Duplicate aliases are suppressed when `USER_IDENTITY == USER_ID` or `PROVIDER_ID == PROVIDER`.

### Inspector kv navigation and copy

Validated behavior:
- Property values can be clickable when they resolve to an existing graph node.
- Resolution checks exact `entityKey`, bare suffix after `:`, and unique node title.
- User nodes are intentionally preferred over other echoes while Hydra runtime identity may still be pending.
- Each kv row has a small `[  ]` copy button.
- Numeric and boolean-like values copy as `key=value`; other values copy as plain value.
- Binary-like detection helpers may remain parked for future copy formatting.

### Dynamic sticky columns

Validated behavior:
- Server and SCSession columns appear when relevant nodes exist.
- Hydra column: always.
- User column: always.
- Party column: appears once a renderable Party exists, then stays for the run.
- MM column: appears once a renderable MM session exists, then stays for the run.
- SCSession is placed between Server and Hydra.

### Party and MM projection

Validated / intended behavior:
- Party reducer handles `Hydra.Api.Push.Presence.PresencePartyUpdate`.
- Party nodes hide when empty.
- MMSession nodes hide when empty.
- Party and MM Y positions are based on average member User Y.
- Overlap nudging keeps multiple groups visually separated.
- Party → MM link reduction:
  - if Party member set matches MMSession member set;
  - and MM leader belongs to that Party;
  - replace direct User → MMSession links with Party → MMSession link.

### SCSession layer

Validated direction:
- SCSession is the SessionControl/game-session layer between Server and Hydra.
- Dedicated Server `serverId` must not be used as an SCSession id.
- Real SC session ids come from `CreateSessionResponse.gameSessionId` and/or `serverContext.data.kernelSessionId`.
- Server → SCSession should be based on observed activation flow, not ID resemblance.
- SCSession → Hydra should be based on observed user-side SessionControl context.

Relevant activation flow:
- `GetServerSessionInfoRequest` stores the pending Dedicated Server id.
- `GetServerSessionInfoResponse` with `sessionInfo` marks that server as pending SC activation.
- `PrepareActivateSessionResponse` contains the real SC id in `serverContext.data.kernelSessionId`.
- The reducer attaches that SC id to the matching server state and creates/updates `SCSessionState`.

Relevant Hydra/user bridge flow:
- `CreateSessionRequest` stores pending user-side context.
- `CreateSessionResponse` uses `gameSessionId` as the SC session id.
- `GetServerInfoRequest` can directly confirm `gameSessionId` plus user-side context.
- `GetSessionEventsRequest/Response` can attach member user contexts to the remembered SC session.

## Workflow History

### Codex instance lanes stabilized

Forge and Lumen lane behavior is considered stable enough to live in durable/background docs rather than dominate `active.md`.

Current lanes:

- **Forge:** graph/reducer/session/projector/Inspector behavior.
- **Lumen:** ImGui texture, mipmap, icon, and visual asset handling.

Durable lane rhythm:
- Every lane starts by reading `AGENTS.md`, `active.md`, and `md-files/interlane.md`, then checking `git status --short --branch`.
- Use `md-files/interlane.md` for live cross-lane notes, requests, warnings, and handoffs.
- Avoid overlapping file edits unless Archy explicitly asks for cross-lane work.
- Promote only stable coordination rules into durable docs.
- Keep `active.md` focused on current product/code work, not ongoing lane housekeeping.

### WSL subset and bridge scripts

The full production app is compiled and run on Win11 in Visual Studio.

This repository is a WSL subset used for focused Codex editing, commits, and review history. Archy bridges code between this subset and the full project with scripts in `bridges/`.

Codex commits here are collaboration checkpoints; Archy reviews, integrates, tests, and makes production-repo commits afterward.

### Line endings

`.gitattributes` normalizes important source and note files to CRLF in the working tree:

- `.h`
- `.hpp`
- `.cpp`
- `.md`
- `.ps1`

This keeps copy/bridge behavior consistent between Ubuntu/WSL and Windows.

### `task_ctx`

`task_ctx/` is intentionally ignored by Git. It is a staging area for logs, LL analysis, and temporary task context that Codex may inspect but should not commit.
