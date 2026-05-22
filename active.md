# ImGui Coordinator — Active Task Context

Last updated: 2026-05-22

This file is the short-lived cockpit for the current Codex work.
`AGENTS.md` is the durable repo entry point; this file is allowed to change often.
Deeper durable docs live in `md-files/`.

## Current

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

## Validated

Accepted baselines live in `md-files/history.md`; deferred work lives in `md-files/tickets.md`.
