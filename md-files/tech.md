# ImGui Coordinator — Technical Notes

Durable technical guidance for the C++ / Dear ImGui Coordinator.

Read `../active.md` first for current focus; this file is background architecture and style.

## UI Structure

The UI is split into four panes:

1. Units panel, left
   - Inventory/navigation pane.
   - Groups graph nodes by `NodeKind`.
   - Clicking an entry selects that node.

2. Graph panel, center
   - Flow visualization canvas.
   - Nodes are rendered as icons + title/subtitle.
   - Links are rendered as curves/arrows.
   - Supports pan/zoom/grid.
   - Selection is shared with side panels.

3. Inspector panel, right
   - Shows selected node metadata.
   - Shows incoming/outgoing links.
   - Shows key/value facts for selected nodes.
   - `GraphNode::kv` is currently the property payload surface.
   - Some kv values are navigable when they resolve to graph nodes.
   - kv rows include copy buttons for evidence extraction.

4. Logs panel, bottom
   - Shows runtime logs.
   - Supports filtering, level toggles, copy, autoscroll.
   - Used heavily during event stream development.

## Main Architecture Rule

Do not mutate the graph directly from packet handlers except through the existing projection flow.

Preferred flow:

1. Parse incoming HTTP request into `SdkPacket`.
2. Dispatch by `SdkPacket::reqNameId`.
3. Reducers update `LiveState`.
4. At the end of the ingestion batch, if anything changed, call the projector once.
5. Projector rebuilds `GraphModel` from `LiveState`.

This avoids dangling links, flicker, and partial graph states.

## Important Node Kinds

Relevant `NodeKind` values include:

- `HydraSample`
- `User`
- `SCSession`
- `Party`
- `MMSession`
- `StandaloneServer`
- `HeatedDSServer`
- older/demo kinds such as `DSSession`, `MMFlowSample`

Current graph story:

```text
HeatedDSServer / StandaloneServer → SCSession → HydraSample → User → Party → MMSession
```

SCSession is the SessionControl/game-session layer between Servers and Hydra.

## Key Files

- `start_server_controller.h`
  - Declares `SdkPacket`, `CoordinatorHttpServer`, `LiveState`, state structs, and controller methods.

- `packets_ingestion.cpp`
  - Ingestion drain/dispatch anchor.

- `packet_json_helpers.h/.cpp`
  - Shared JSON extraction helpers such as `NodeAt`, `StrAt`, `BoolAt`, identity/session extraction, and path helpers.

- `users_ingestion.cpp`
  - Sign-in/sign-out and user facts.

- `presence_ingestion.cpp`
  - Party and MMSession reducers, including disband/leave requests.

- `servers_ingestion.cpp`
  - Dedicated-server, SessionControl, and standalone-server reducers/correlation.
  - Handles Dedicated Server state and SessionControl flows; links Server → SCSession only when activation flow proves the SC id.

- `projector.cpp` and `projector_*`
  - Convert `LiveState` into `GraphModel`.
  - Place Server/SCSession/Hydra/User/Party/MM nodes.
  - Project links when reducer state proves relationships.
  - Apply Party → MM link reduction.

- `graph_types.h/.cpp`
  - Define `GraphModel`, `GraphNode`, `GraphLink`, `NodeKind`, ports, link style, and graph lookup helpers.

- `inspector_panel.cpp/.h`
  - Renders selected node details.
  - Displays `GraphNode::kv`.
  - Handles kv navigation and copy buttons.

## Packet Dispatch

Incoming HTTP packets include a request name. The project maps request names into generated enum IDs (`ReqNameID`) and dispatches on `SdkPacket::reqNameId`.

Use the enum, not string-prefix checks.

Example style:

```cpp
switch (u.reqNameId) {
   case HYDRA_API_PUSH_PRESENCE_PRESENCESESSIONUPDATE:
      return HandleMMSessionUpdate(u);

   case HYDRA_API_PUSH_PRESENCE_PRESENCEPARTYUPDATE:
      return HandlePartyUpdate(u);

   case PROS_API_FACTS_WRITEBINARYPACKUSERREQUEST:
      return HandleFactsWriteBinaryPackUser(u);
}
```

Unknown request names should be surfaced so they can be added to `ReqNames.txt`, then regenerated into the enum/header pipeline.

## Facts And Identity

`Pros.Api.Facts.WriteBinaryPackUserRequest` carries useful `propertyName` / `propertyValue` pairs. These are stored in state and projected into `GraphNode::kv`.

SDK identity naming is inconsistent. Important values include:

- `context.data.userIdentity`
- `member.userId`
- Facts `USER_ID`
- Facts `RUNTIME_SEANCE_ID`
- user/client `kernelSessionId`

Do not assume identity-like values are the same without evidence.

Local vs remote:
- The Coordinator observes HTTP traffic from local instances of apps and
  servers.
- That traffic may mention other apps, servers, users, or sessions; those
  mentioned entities are remote unless reducer evidence proves they are local.
- `UserState::isLocal` currently exists as a placeholder display flag. Forge
  should replace its default with concrete reducer evidence when available.

## SessionControl / SCSession

`NodeKind::SCSession` represents the SessionControl game-session layer.

Reliable linking rules:

- Dedicated-server handshake/session info creates the Server node, but `serverId` is not an SC session id.
- `GetServerSessionInfoRequest` stores the pending Dedicated Server id.
- `GetServerSessionInfoResponse` with `sessionInfo` marks that server as pending SC activation.
- `PrepareActivateSessionResponse` contains the real SC id in `serverContext.data.kernelSessionId`.
- Server → SCSession is linked only when activation flow proves `ServerState::scSessionId`.
- `CreateSessionRequest` stores pending user-side context.
- `CreateSessionResponse` uses `gameSessionId` as the SC session id.
- `GetServerInfoRequest` can directly confirm `gameSessionId` + user-side context.
- `GetSessionEventsRequest/Response` can attach member user contexts to the remembered SC session.

Never use Dedicated Server `serverId` as an SC session id.

## Hydra Identity

HydraSample nodes represent the Hydra/client runtime entry for a user.

Hydra identity priority:

1. `UserState::runtimeSeanceId` from Facts `RUNTIME_SEANCE_ID`.
2. user/client `hydraKernelSessionId`.
3. user id fallback.

Use shared projector helpers for Hydra node ids/entity keys so links and nodes stay aligned.

## Party / MM

Party handling is based on `Hydra.Api.Push.Presence.PresencePartyUpdate`.

Party reducer should:
- identify `partyId` from packet data when available;
- apply member deltas;
- track members by user ID;
- track leader/owner;
- track join code/settings when available;
- hide empty parties but keep the Party column sticky once shown.

MM handling is based mainly on `Hydra.Api.Push.Presence.PresenceSessionUpdate`.

MM reducer should:
- track session ID;
- track members;
- track owner;
- track state such as QUEUE/GAME;
- hide empty sessions but keep MM column sticky once shown.

Party → MM link reduction:
- if Party member set matches MMSession member set;
- and MM leader belongs to that Party;
- replace direct User → MMSession links with Party → MMSession link.

## Layout Rules

Server/SCSession/Hydra/User/Party/MM nodes are placed in columns.

SCSession is placed between Server and Hydra.

Users have stable Y positions based on first-seen order.

Party and MM session Y positions are derived from the average Y position of their member users.

Overlap is resolved by nudging later items down.

Use the shared centroid helpers such as:

- `LiveState::YforSession(...)`
- `LiveState::YforParty(...)`
- `LiveState::YforGroupAsCentroid(...)`

Do not replace this with ad hoc positioning unless asked.

## Inspector Behavior

`GraphNode::kv` rows should stay factual and named.

Do not dump huge endpoint/config arrays into kv.

Do not store or display tokens/secrets.

Clickable kv values resolve through graph node echoes:
- exact `entityKey`;
- bare suffix after `:`;
- unique title;
- User nodes may be preferred while Hydra runtime identity is pending.

Copy button behavior:
- numeric and boolean-like values copy as `key=value`;
- other values copy as plain value;
- binary-like helpers may remain parked for future formatting.

Do not introduce `GraphPropertySection` until `GraphNode::kv` is clearly insufficient.

## Logging And Review

Logs are part of the development workflow.

## WSL Setup Health

Doctor lane owns setup-health checks for the WSL Codex surface. Start with the
direct WSL repo path and ordinary shell/git/tool commands before trying
workarounds.

Current environment evidence:
- `bridges/nvm_install_log.txt` records NVM installation of Node v24.16.0 under
  `/home/miron/.nvm`.
- That log shows WSL `node`/`npm` paths taking precedence over Windows Node/npm
  paths, which is the desired direction for WSL-local Codex operation.

## Texture LODs

Graph node icons use `TextureManager::IconForKind(NodeKind, desiredPx)`.

Some `NodeKind` values are pure visual LOD keys rather than graph entities. This
covers UI and status/control families such as connector caps/crosses,
local/offline/online status markers, and party leader markers. These keys may be
used with `IconForKind` / `LodInfoForKind(NodeKind, desiredPx)`, but should not
be projected as graph nodes unless a product task explicitly asks for that.

User badges:
- `GraphNode::badges` stores visual marker keys drawn as overlays on the primary
  icon.
- User badges currently cover online/offline, owner/leader role, and local-user
  state.
- Badges are visual annotations, not graph entities or links.
- Keep all badges the same on-screen size at a given zoom level.

## Bridge Workflow

The full production project lives on Win11 and includes Visual Studio project files and additional source files.

This WSL repository is a small Codex-focused subset used for focused editing, review, and Git history.

Archy bridges code between the WSL subset and the full Win11 project with scripts in `bridges/`.

Current bridge convention:

- Forge/core files mirror flat at repo root.
- Lumen visual/texture files mirror flat under `Lumen/`.
- Preferred bridge entry point is `bridges/bridge.ps1`:
  - `.\bridge.ps1 -Direction ToCodex`
  - `.\bridge.ps1 -Direction ToWin`
- Older one-way bridge scripts remain for compatibility while `bridge.ps1` is tested.
- Bridge scripts should keep matching Lumen file arrays.
- Codex commits here are review artifacts and collaboration checkpoints.
- Archy reviews, compiles, runs, and makes production-repo commits afterward.

Do not worry about `.vcxproj` updates unless explicitly asked; Archy often handles them in the Win11 project.

## Style Preferences

- Prefer small, focused changes.
- Preserve existing architecture unless there is a strong reason to refactor.
- Keep reducers pure-ish: update state, do not directly draw.
- Keep projection deterministic.
- Prefer readable C++ over clever abstractions.
- Avoid broad rewrites.
- Use existing helper style (`NodeAt`, `StrAt`, `BoolAt`, etc.) if present.
- If unsure about SDK semantics, add a small defensive helper or log line rather than guessing.
