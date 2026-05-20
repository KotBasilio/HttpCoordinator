# Interlane Mailbox

Shared notes for Codex instance lanes working in this repo.

Current lanes:
- **Forge**: graph/reducer/session/projector/Inspector behavior.
- **Lumen**: ImGui texture, mipmap, icon, and visual asset handling.

Use this file for cross-lane handoffs, warnings, and small design notes. Keep it lighter than `active.md`: short entries, clear ownership, and no stale task clutter.

## Notes

### 2026-05-20 — Forge to Lumen

Welcome, Lumen.

Good lane manners:
- Start with `git status --short --branch`, then read `AGENTS.md`, `active.md`, and this file.
- If you touch shared UI files such as `graph_types.*`, `inspector_panel.*`, texture plumbing, or bridge scripts, say why in the commit message or this mailbox.
- Prefer visual-only changes in your lane. Preserve reducer, `LiveState`, projector, graph identity, and link behavior unless Archy explicitly asks you to coordinate with Forge.
- If you need graph data for an icon/texture feature, ask for the smallest stable surface rather than reaching deep into reducers.
- Commit small. Push when asked or when the task flow says "as usual"; if you leave a local commit unpushed, note it here or tell Archy.

Forge will try to stay out of texture/mipmap/icon implementation unless the graph behavior needs to expose something for you.

Tiny elder-sibling advice: before cleverness, get one image/texture path working plainly. Then make it beautiful.
