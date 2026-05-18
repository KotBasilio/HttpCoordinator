# ImGui Coordinator — Active Task Context

Last updated: 2026-05-18

This file is the short-lived cockpit for the current Codex work.
`AGENTS.md` is the durable repo guidance; this file is allowed to change often.

## Current

### Current focus

Continue the Coordinator graph evolution work around real Hydra / SDK event streams.

Current graph story:

```text
Server → SCSession → HydraSample → User → Party → MMSession
```

The tool should help us understand multiplayer/session behavior by turning noisy request/push logs into a stable visual graph.

### Current priority

#### 1. Validate SCSession linking against real SessionControl traffic

The latest direction is evidence-based Server/SC/Hydra linking:

- Dedicated Server `serverId` is **not** an SC session id.
- Real SC session ids come from:
  - `CreateSessionResponse.gameSessionId`
  - `serverContext.data.kernelSessionId`
- User/Hydra-side context comes from:
  - `userContext.data.userIdentity`
  - `userContext.data.kernelSessionId`
  - similar `context.data.*` paths when present

Current expected visual chain:

```text
HeatedDSServer / StandaloneServer → SCSession → HydraSample → User
```

Validation questions:
- Does `GetServerSessionInfoRequest/Response` followed by `PrepareActivateSessionResponse` create exactly one Server → SCSession link for the tested run?
- Does `CreateSessionRequest/Response` or `GetServerInfoRequest` create SCSession → HydraSample only when user-side context is observed?
- Are extra SCSession nodes being created from non-SC IDs?
- Are stale pending correlation fields cleared or harmless for the current ordered stream?

#### 2. Prepare richer node property tables

Next UI direction:

- Keep `GraphNode::kv` as the projection payload surface unless pressure demands a stronger model.
- Make Inspector property tables richer and more node-specific.
- Prefer factual reducer state over transient/noisy values.

Useful first targets:
- Server: server id, kind, modern API flag, linked SC session id, server name / standalone relation when present.
- SCSession: game session id, server-context kernel session id, linked server id, Hydra user identity, Hydra kernel session id, member count.
- User: user id, nickname, online state, kernel session id, Facts key/value payload.
- Party/MM: stable ids, member count, owner/leader, join code/state, reduced link explanation when useful.

#### 3. Keep Party/MM behavior stable

Existing Party/MM logic should remain stable while SC work continues.

Do not redesign Party/MM unless the task explicitly asks for it.

## Validated

These are recent accepted/working baselines. They are useful context, not all current work.

### Ingestion and reducer/projector architecture

Validated baseline:

1. HTTP request arrives.
2. Request becomes `SdkPacket`.
3. Dispatch happens by `SdkPacket::reqNameId`.
4. Reducers update `LiveState`.
5. At the end of the ingestion drain batch:
   - if anything changed, call projector once.
6. Projector clears/rebuilds `GraphModel` deterministically.

Do not directly hand-edit graph nodes from packet handlers.

### User facts in Inspector

Validated behavior:
- `Pros.Api.Facts.WriteBinaryPackUserRequest` carries useful `propertyName` / `propertyValue` pairs.
- These are projected into `GraphNode::kv`.
- Inspector displays these pairs for selected nodes, especially User nodes.

### Dynamic sticky columns

Validated behavior:
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
- Server → SCSession should be based on observed activation flow, not ID resemblance.
- SCSession → Hydra should be based on observed user-side SessionControl context.

## Deferred

These are known ideas/seams that should not be treated as current tasks unless explicitly promoted.

### Identity seam

The SDK uses multiple identity-like values:

- `context.data.userIdentity`
- `member.userId`
- Facts `USER_ID`

Do not assume they are the same.

Possible future improvement:
- maintain an identity map from `userIdentity` to canonical `USER_ID` when Facts packets reveal both.

### Multi-slot correlation

Some current SC correlation uses single pending fields because observed logs are ordered.

If future logs interleave multiple servers/sessions, replace single pending fields with a queue/map keyed by stronger correlation evidence.

### Stronger Party ↔ MMSession binding

Future stronger binding may use Facts packets that contain both:

- `PARTY_SESSION_ID`
- `MATCHMAKE_SESSION_ID`

Until then, keep the current member-set heuristic.

### Richer property table model

If `GraphNode::kv` becomes insufficient, consider a structured model such as:

```cpp
struct GraphPropertySection {
   std::string title;
   std::vector<std::pair<std::string, std::string>> rows;
};
```

Do not introduce this before pressure appears.

### Historical replay UI

Do not implement a full historical replay UI unless explicitly asked.

### Global layout redesign

Do not redesign the whole layout system unless explicitly asked.

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

## Coding style reminders

- Prefer small, reviewable patches.
- Preserve reducer/projector separation.
- Prefer stable, readable C++ over cleverness.
- Use existing helper style (`NodeAt`, `StrAt`, `BoolAt`, etc.) if present.
- Avoid broad refactors unless explicitly requested.
- If adding a new packet handler, dispatch via `SdkPacket::reqNameId`.
- If adding a new request name, update the request-name generation pipeline rather than string-prefix hacks.
- Do not collapse identity domains without evidence.

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

Codex may commit and push in this repository freely. Small commits are preferred because the human owner reviews Codex changes through Git.

Workflow context:
- The full app (ImGui Coordinator) is compiled and run on Win11 in the production Visual Studio project.
- This WSL repository is a small Codex working subset. It may omit `.vcxproj` files and some UI/source files from the full project.
- The human owner (named Archy) bridges code between this subset and the Win11 project with local scripts.
- Codex commits here are intended to make review/history easy; the human owner handles production integration after review.
- Relevant scripts/bridges are located in the `bridges` folder. You don't need to run them. They are provided for Archy's convenience and for context about the bigger workflow.
