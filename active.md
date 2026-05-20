# ImGui Coordinator — Active Task Context

Last updated: 2026-05-20

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

#### 0. Coordinate Codex instance lanes

This repo may be worked on by more than one Codex instance.

- **Forge**: primary graph/reducer/session implementation lane. Its lane folder is `Forge/`.
- **Lumen**: ImGui texture, mipmap, icon, and visual asset handling lane. Its lane folder is `Lumen/`.

Both lanes share the same repo and Git history. Before editing, each instance should
read `AGENTS.md` and `active.md`, check the working tree, and avoid overlapping
file edits unless the task explicitly crosses lanes.

Use `interlane.md` as the shared mailbox for cross-lane notes, requests, warnings,
and handoffs. Keep entries short, dated when useful, and signed by lane name.

Lumen should preserve existing graph/reducer/projector behavior unless a visual
task explicitly requires coordination with those systems.

#### 1. Keep rich node properties useful and factual

Recent work populated `GraphNode::kv` for Server, SCSession, User, HydraSample, Party, and MMSession nodes.

Current Inspector polish:
- kv values can act as graph navigation links when they echo existing graph nodes;
- User nodes are intentionally preferred when a pending Hydra identity still collides with a User id;
- each kv row has a small copy button for fast evidence extraction.

Next likely refinement:
- keep adding only stable, named facts observed in reducer state;
- omit empty placeholders and duplicate identity aliases;
- avoid tokens, raw endpoint dumps, and transient/noisy values;
- keep Inspector backed by `GraphNode::kv` until a stronger property-section model is truly needed.

#### 2. Preserve graph behavior while iterating

Current graph story remains:

```text
Server → SCSession → HydraSample → User → Party → MMSession
```

Existing Party/MM and SCSession behavior should remain stable unless a task explicitly promotes a change.

## Validated

Accepted baselines live in `history.md` so this cockpit stays short.

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

- `graph_types.h`
- `projector*`

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
