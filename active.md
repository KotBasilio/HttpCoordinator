# ImGui Coordinator — Active Task Context

Last updated: 2026-05-22

This file is the short-lived cockpit for the current Codex work.
`AGENTS.md` is the durable repo entry point; this file is allowed to change often.
Deeper durable docs live in `md-files/`.

## Current for Forge lane

### Current focus 

Continue the Coordinator graph evolution work around real Hydra / SDK event streams.

Current graph story:

```text
Server → SCSession → HydraSample → User → Party → MMSession
```

The tool should help us understand multiplayer/session behavior by turning noisy request/push logs into a stable visual graph.

### Current priorities

- Keep selected-node properties useful and factual.
- Preserve Party/MM, SCSession, and projector behavior while iterating.
- Promote deferred ideas from `md-files/tickets.md` only when a task or new evidence calls for them.

## Current for Lumen lane

### Current focus

Lumen is the visual/texture lane for ImGui graph presentation: texture loading,
icon LOD selection, mipmap-facing behavior, graph/units visual plumbing, and
asset bridge alignment.

Use MV-M voice handles when helpful:

- **Bob ☕🧩:** technical implementation/review, compile risks, texture and LOD
  correctness, sharp humor.
- **Trace 🫖🧭:** architecture, scope, pacing, continuity, and whether visual
  changes preserve the graph story.

### Current priorities

- Keep Lumen changes in `Lumen/` where possible.
- Treat `graph_types.*`, `inspector_panel.*`, and bridge scripts as shared
  surfaces; leave interlane context when touching them.
- Preserve Forge-owned reducer -> `LiveState` -> projector -> `GraphModel`
  behavior and graph identity.
- Visual-only `NodeKind` values may be used as texture/LOD keys, not projected
  graph entities unless a task explicitly asks for that.
- Keep bridge arrays aligned with Lumen visual files.
