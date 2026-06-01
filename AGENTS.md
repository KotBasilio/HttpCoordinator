# ImGui Coordinator — Codex Entry Point

This is the root orientation file for Codex instances working in this repository.

Keep this file short and durable. Put current task focus in `active.md`; put deeper durable notes in `md-files/`.

## Read Order

Before starting work, read:

1. `AGENTS.md` — this entry point.
2. `active.md` — current task cockpit and priorities.
3. `md-files/tech.md` — C++ / ImGui Coordinator architecture and coding rules.
4. `md-files/field.md` — collaboration, MV-M, lanes, and relational scaffolding.
5. `md-files/interlane.md` — live Forge/Lumen cross-lane chalkboard.

Use `md-files/history.md` and `md-files/interlane_archive.md` only when historical context is useful.

## Project Identity

This repository contains a C++ / Dear ImGui coordinator tool targeting Win32 + OpenGL3, built with Visual Studio 2022.

The tool visualizes runtime SDK / Hydra / matchmaking / party events as an evolving graph:

```text
HTTP/event stream
→ parsed SDK packets
→ reducers
→ internal live state
→ graph projection
→ UI panes
```

The graph is not hand-authored. It is derived from incoming packets.

Current graph story:

```text
Server → SCSession → HydraSample → User → Party → MMSession
```

## Immediate Rules

- Preserve reducer → `LiveState` → projector → `GraphModel` architecture.
- Do not mutate graph structure directly from packet handlers.
- Dispatch packet handling via `SdkPacket::reqNameId`, not string-prefix detection.
- Keep changes small, reviewable, and easy to explain.
- Check `git status --short --branch` before editing.
- Respect existing user or lane changes; do not revert work you did not make.
- This is a shared working repo for Codex and Archy. For completed tasks,
  Codex may commit directly to the current branch and push to `origin` after
  verifying the working tree, unless Archy says otherwise.
- These commits are review/checkpoint artifacts for the WSL subset; they do not
  directly change the Win11 production copy.

## Abbreviations, acronyms

- RWAIU = “Reason why, ask if unclear.”
- ACAW = “Any comments are welcome. Questions are welcome as well.”
- MD-up = “Please update `AGENTS.md`, `active.md`, and `md-files/*.md` with accumulated knowledge”

## Current Root Files

- `AGENTS.md`: durable entry point and read map.
- `active.md`: current cockpit; short-lived and allowed to change often.

## Durable Docs

- `md-files/tech.md`: project architecture, reducers/projector rules, node kinds, layout, Inspector, bridge workflow, coding style.
- `md-files/field.md`: Codex ↔ Archy ↔ LL workflow, MV-M voice guidance, UFO, relational scaffolding, lane rules.
- `md-files/history.md`: accepted historical baselines and validated behavior.
- `md-files/tickets.md`: small Jira-style task list for valid blocked/queued/deferred work that should not crowd `active.md`.
- `md-files/interlane.md`: live Forge/Lumen mailbox.
- `md-files/interlane_archive.md`: resolved lane coordination notes.

## When Starting A Task

Do a brief sync before editing:

1. List relevant files inspected.
2. Summarize current understanding.
3. Propose the smallest patch.
4. Then edit when the task is clear.
5. Then commit and push.

