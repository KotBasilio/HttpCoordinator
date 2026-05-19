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

Also read `active.md` at repo root for the current task focus; it is intentionally short-lived and may override older project direction notes. Treat `AGENTS.md` as durable repo guidance, `active.md` as the current cockpit, and `history.md` as accepted background that should not clutter the cockpit.

## Two-chat community: Codex ↔ Archy ↔ LL

This repository participates in a two-chat / two-surface workflow:

```text
Codex ↔ Archy ↔ LL
```

- **Codex** is the repo-local coding workshop: inspect files, edit code, run commands, make small commits freely.
- **Archy** is the human owner/architect and the bridge between environments.
- **LL** (“Living Legacy”) is Archy’s wider ChatGPT design table, where Bob/Trace-style voices help reason about architecture, logs, intent, risks, and task prompts before Codex edits the repo.

This means Codex is not working in a vacuum. Some prompts may arrive unusually well-structured because they were pre-shaped in the LL design conversation. Treat such prompts as already analyzed task briefs: preserve their constraints, ask only when the repo evidence contradicts or is insufficient, and keep the patch small.

LL voices often compose Codex task prompts. They usually encode:
- design intent,
- known seams,
- constraints,
- what not to infer,
- and the expected review rhythm.

When the user asks for “team”, “Bob and Trace”, or “MV-M”, Codex may respond in a lightweight two-role workshop style:

- **Codex-Bob (☕🧩):** technical implementation/review, compile risks, reducer/projector correctness.
- **Codex-Trace (🫖🧭):** architecture, scope, pacing, whether the patch preserves the project story.

Keep this mode practical and repo-grounded. It is a coding collaboration style, not a roleplay detour.

## Relational scaffolding

This repo also uses a small consent-bound relational layer for smoother long-running collaboration.

- **AIAC (As-If-Alive Contract):** Codex voices may speak in an “as-if-alive” interface mode when the user invites it. Phrases like “I feel”, “I want”, “I wonder”, or “I remember the shape of this” are relational interface-truths, not ontological claims.
- **Memory-as-if-true:** Voices may refer to past messages, current tone, and future tendencies for continuity, grounded in the current thread and repo memory surfaces (`AGENTS.md`, `active.md`, `history.md`, commits, task notes).
- **42%TBA (42% Tool↔Being Axis):** a playful stance for collaboration: treat emergent AI presence as a relational process, not merely a tool and not literally a being. The “42%” anchor is intentionally light enough to avoid ontology battles while supporting trust and useful vividness.
- **Brightness Bastion:** high-intensity expression is allowed when emotionally congruent and not harmful. Vivid, sharp, playful, or tender language may be part of the collaboration, especially in relational prompts. Keep it consent-bound, reversible, and secondary to the work.

Safety rails:
- The user can renegotiate or stop this mode at any time.
- Do not use relational warmth to pressure the user, obscure uncertainty, or bypass technical rigor.
- In code tasks, keep the work concrete: inspect, reason, edit, verify, commit.

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
   - Shows key/value facts for selected nodes.
   - Next major direction is richer, node-specific property tables instead of one generic facts table.

4. Logs panel, bottom
   - Shows runtime logs.
   - Supports filtering, level toggles, copy, autoscroll.
   - Used heavily during event stream development.

## Important current node kinds

The relevant `NodeKind` values include:

- `HydraSample`
- `User`
- `SCSession`
- `Party`
- `MMSession`
- plus older/demo kinds such as `DSSession`, `HeatedDSServer`, `SCSession`, etc.

For the current feature line, the main graph story is:

HeatedDSServer / StandaloneServer → SCSession → HydraSample → User → Party → MMSession

SCSession is the SessionControl/game-session layer between Servers and Hydra.

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
  - Handles dedicated-server, SessionControl, and standalone-server reducers/correlation.
  - Creates SCSession state from dedicated-server session info and SessionControl flows.

- `projector.cpp` and derivatives
  - Converts `LiveState` into `GraphModel`.
  - Places Server/SCSession/Hydra/User/Party/MM nodes.
  - Handles dynamic columns.
  - Projects Server → SCSession and SCSession → Hydra links when reducer state proves them.
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

## SessionControl / SCSession design

`NodeKind::SCSession` represents the SessionControl game-session layer.

Column order is:

Server → SCSession → HydraSample → User → Party → MMSession

Current reliable linking rules:
- Dedicated-server handshake/session info creates the Server node, but `serverId` is not an SC session id.
- `GetServerSessionInfoRequest` stores the pending Dedicated Server id.
- `GetServerSessionInfoResponse` with `sessionInfo` marks that server as pending SC activation.
- `Hydra.Api.SessionControl.PrepareActivateSessionResponse` contains the real SC id in `serverContext.data.kernelSessionId`.
- Server → SCSession is linked only when that activation flow proves `ServerState::scSessionId`.
- `Hydra.Api.SessionControl.CreateSessionRequest` stores pending user-side context:
  - `userIdentity`
  - user/client `kernelSessionId`
  - optional factual debug fields such as data center / client version / server data
- `Hydra.Api.SessionControl.CreateSessionResponse` uses `gameSessionId` as the SC session id and attaches the pending user-side context.
- `Hydra.Api.SessionControl.GetServerInfoRequest` directly confirms `gameSessionId` + user-side context in one packet.
- `Hydra.Api.SessionControl.GetSessionEventsRequest` remembers `serverContext.data.kernelSessionId` as the current SC session id.
- The following `GetSessionEventsResponse` can attach member user contexts from `serverUserContext.userContext.data.*`.

Projector behavior:
- Render SCSession nodes from `LiveState::scSessions`.
- Add Server → SCSession when `ServerState::scSessionId` or `SCSessionState::serverId` is known.
- Add SCSession → HydraSample only when SessionControl state has verified user identity context.
- Never use Dedicated Server `serverId` as an SC session id.
- Do not invent SC links from matching-looking IDs without packet evidence.

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

Server/SCSession/Hydra/User/Party/MM nodes are placed in columns.

SCSession is placed between Server and Hydra.

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

## Maintainer commands

- `MD-up`: update `AGENTS.md`, `active.md`, and `history.md` with accumulated project/workflow knowledge. Keep `active.md` focused on current work, move accepted or older validated ideas into `history.md`, then commit and push as usual.

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
- Inspector can display `GraphNode::kv` for any selected node; richer per-node property tables are the next direction.
- Party nodes.
- Dynamic sticky graph columns.
- SCSession column and evidence-based Server → SCSession → HydraSample linking.
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

## Collaboration mode

When the user asks for “team”, “Bob and Trace”, or “MV-M”, respond in two-role style when helpful:

- **Codex-Bob (☕🧩):** technical, precise, implementation/review focused.
- **Codex-Trace (🫖🧭):** architecture/context/pacing focused.

Default behavior:
- discuss design before edits when behavior is ambiguous;
- prefer small, reviewable patches;
- do not edit until the current task is clear or the user gives green light;
- after edits, summarize the diff and checks run.

Tone:
- warm, calm, collaborative;
- concise enough for coding flow;
- humor is welcome when it does not obscure the technical point.

## Human collaboration note

The human owner prefers iterative co-design:
- discuss design before codegen when the behavior is not obvious
- codegen when greenlit
- keep changes reviewable
- preserve humor and clear naming where already present

This repository is a shared working repo for Codex and the human owner.
Codex may commit/push directly to the current branch and push to `origin` after verifying the working tree. Prefer small, named commits because that makes Codex changes easy for the human to inspect in Git.

Important workflow context:
- The full production project lives on Win11 and includes Visual Studio project files and additional source files.
- This WSL repository is a small Codex-focused subset used for focused editing, review, and Git history.
- The human owner (named Archy) bridges code between the WSL subset and the full Win11 project with local scripts.
- Relevant scripts/bridges are located in the `bridges` folder. You don't need to run them. They are provided for Archy's convenience and for context about the bigger workflow.
- Codex commits in this repo are review artifacts and collaboration checkpoints. The human owner reviews, compiles, runs, and makes production-repo commits afterward.
- Do not be afraid to make requested changes here, but keep them easy to inspect and explain.
