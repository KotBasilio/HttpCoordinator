# ImGui Coordinator — Project Brief for Codex

## Project identity

This repository contains a C++ / Dear ImGui coordinator tool targeting Win32 + OpenGL3, built with Visual Studio 2022.

The tool visualizes runtime SDK / Hydra / matchmaking / party events as an evolving graph. Its goal is to turn noisy event streams into a stable, inspectable visual model for debugging and understanding multiplayer/session behavior.

Think of the app as:

HTTP/event stream
→ parsed SDK packets
→ reducers
→ internal live state
→ graph projection
→ UI panes

The graph is not hand-authored. It is derived from incoming packets.

Also read `active.md` at repo root for the current task focus; it is intentionally short-lived and may override older project direction notes.

## Current UI structure

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
   - Shows key/value facts for selected user nodes.

4. Logs panel, bottom
   - Shows runtime logs.
   - Supports filtering, level toggles, copy, autoscroll.
   - Used heavily during event stream development.

## Important current node kinds

The relevant `NodeKind` values include:

- `HydraSample`
- `User`
- `Party`
- `MMSession`
- plus older/demo kinds such as `DSSession`, `HeatedDSServer`, `SCSession`, etc.

For the current feature line, the main graph story is:

HydraSample → User → Party → MMSession

HydraSample and User columns are always present.
Party and MMSession columns are dynamic/sticky per run:
- Party column appears once a renderable Party exists.
- MM column appears once a renderable MM session exists.
- Once visible in a run, the column remains visible to avoid layout jitter.

## Main architecture rule

Do not mutate the graph directly from packet handlers except through the existing projection flow.

Preferred flow:

1. Parse incoming HTTP request into `SdkPacket`.
2. Dispatch by `SdkPacket::reqNameId`.
3. Reducers update `LiveState`.
4. At the end of the ingestion batch, if anything changed, call the projector once.
5. Projector rebuilds `GraphModel` from `LiveState`.

This is intentional. It avoids dangling links, flicker, and partial graph states.

## Key files / concepts

Likely important files:

- `start_server_controller.h`
  - Declares `SdkPacket`, `CoordinatorHttpServer`, `LiveState`, `UserState`, `PartyState`, `SessionState`, and controller methods.

- `packets_ingestion.cpp`
  - Ingestion split anchor.
  - Domain reducers live in `users_ingestion.cpp`, `presence_ingestion.cpp`, and `servers_ingestion.cpp`.

- `packet_json_helpers.h` / `packet_json_helpers.cpp`
  - Shared JSON extraction helpers such as `NodeAt`, `StrAt`, `BoolAt`, identity/session extraction, and small path helpers.

- `users_ingestion.cpp`
  - Handles sign-in/sign-out and user facts packets.

- `presence_ingestion.cpp`
  - Handles Party and MMSession reducers, including disband/leave requests.

- `servers_ingestion.cpp`
  - Handles dedicated-server and standalone-server reducers/correlation.

- `projector.cpp`
  - Converts `LiveState` into `GraphModel`.
  - Places Hydra/User/Party/MM nodes.
  - Handles dynamic columns.
  - Applies link reduction such as Party → MM when Party members match MM session members.

- `graph_types.h`
  - Defines `GraphModel`, `GraphNode`, `GraphLink`, `NodeKind`, ports, link style, etc.

- `inspector_panel.cpp`
  - Renders selected node details.
  - Displays `GraphNode::kv` key/value facts.

## SdkPacket / event parsing

Incoming HTTP packets include a request name. The project maps request names into generated enum IDs (`ReqNameID`) and dispatches on `SdkPacket::reqNameId`.

Use the enum, not string-prefix checks, when adding handlers.

Example style:

```cpp
switch (u.reqNameId) {
   case HYDRA_API_PUSH_PRESENCE_PRESENCESESSIONUPDATE:
      return HandleMMSessionUpdate(u);

   case HYDRA_API_PUSH_PRESENCE_PRESENCEPARTYUPDATE:
      return HandlePartyUpdate(u);

   case PROS_API_FACTS_WRITEBINARYPACKUSERREQUEST:
      return HandleFactsWriteBinaryPackUser(u);

   // ...
}
```

Unknown request names are intentionally surfaced so they can be added to `ReqNames.txt`, then regenerated into the enum/header pipeline.

## Facts packets

`Pros.Api.Facts.WriteBinaryPackUserRequest` carries useful `propertyName` / `propertyValue` pairs.

These are displayed in the Inspector for selected User nodes.

Important: SDK identity naming is inconsistent.
There may be several IDs:
- `context.data.userIdentity`
- `member.userId`
- `USER_ID` in Facts header/context

Do not assume these are the same without checking. This identity seam is one of the main reasons this Coordinator tool exists.

## Party / MM design

Party handling is based on `Hydra.Api.Push.Presence.PresencePartyUpdate`.

Party reducer should:
- identify `partyId` from packet data when available
- apply member deltas
- track members by user ID
- track leader/owner
- track join code/settings when available
- hide empty parties but keep the Party column sticky once shown

MM session handling is based mainly on `Hydra.Api.Push.Presence.PresenceSessionUpdate`.

MM reducer should:
- track session ID
- track members
- track owner
- track state such as QUEUE/GAME
- hide empty sessions but keep MM column sticky once shown

A useful visual heuristic already exists / is desired:

If an MM session has the same member set as a Party, and the MM leader is in that Party:
- remove direct User → MMSession links
- add Party → MMSession link
- keep the MM session node visible because state membership still exists

This makes the graph read as “Party goes matchmaking” instead of a tangle of individual user links.

## Layout rules

Hydra/User/Party/MM nodes are placed in columns.

Users have stable Y positions based on first-seen order.

Party and MM session Y positions are derived from the average Y position of their member users.

Overlap is resolved by nudging later items down.

There is a shared abstraction for this idea:
- `LiveState::YforSession(...)`
- `LiveState::YforParty(...)`
- possibly backed by a generic helper like `YforGroupAsCentroid(...)`

Do not replace this with ad hoc positioning unless asked.

## Logging and debug workflow

Logs are part of the development workflow.

The project uses `#pragma message("... REV: ...")` stamps to identify file versions during review.

The human workflow uses:

- `↔️G-sync`: confirm which files/revisions are actually being reviewed.
- `⛓️G-rev`: review the synced code.
- `🔏G-lock`: mark a reviewed baseline as accepted.

Respect these labels in comments or task notes if they appear. Do not assume stale code is current.

## Style preferences

- Prefer small, focused changes.
- Preserve existing architecture unless there is a strong reason to refactor.
- Keep reducers pure-ish: update state, do not directly draw.
- Keep projection deterministic.
- Prefer readable C++ over clever abstractions.
- Avoid broad rewrites.
- When unsure about SDK semantics, add a small defensive helper or log line rather than guessing.

## Current project direction

The current active line is improving graph evolution from real Hydra/SDK event streams.

Recent / active features include:
- User facts displayed in Inspector.
- Party nodes.
- Dynamic sticky graph columns.
- Party → MM link reduction.
- Handling Party disband and MM leave requests.
- Investigating identity mapping among `userIdentity`, `userId`, and Facts `USER_ID`.

## When starting a task

Before editing:
1. Inspect relevant files.
2. Identify current `#pragma message` revision stamps.
3. Summarize what you believe is current.
4. Propose the minimal patch.
5. Then edit.

If the task touches event parsing, inspect representative logs or existing reducer handlers first.

If the task touches layout, inspect `projector.cpp` first.

If the task touches selected-node display, inspect `inspector_panel.cpp` and `graph_types.h`.

## Human collaboration note

The human owner prefers iterative co-design:
- discuss design before codegen when the behavior is not obvious
- codegen when greenlit
- keep changes reviewable
- preserve humor and clear naming where already present

This repository is a shared working repo for Codex and the human owner.
When the human asks for commits/pushes, Codex may commit directly to the current branch and push to `origin` after verifying the working tree. Prefer small, named commits because that makes Codex changes easy for the human to inspect in Git.
