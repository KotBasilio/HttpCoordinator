# Bridge Scripts

These scripts are Archy-operated helpers for moving the small Codex WSL subset
to and from the full Win11 Coordinator repository.

Preferred entry point:

```powershell
.\bridge.ps1 -Direction ToCodex
.\bridge.ps1 -Direction ToWin
```

Older one-way scripts remain for compatibility while `bridge.ps1` is tested.

## Paths

`bridge.ps1` has built-in defaults, then optionally reads:

```text
D:\miron\wip\codex_paths_config.txt
```

Expected config keys:

```text
WinCoordinatorDir=C:\projects\imgui-coo
CodexCoordinatorDir=\\wsl.localhost\Ubuntu\home\miron\proj\HttpCoordinator
TempDir=D:\miron\wip\tmp
```

If the file is missing, defaults are used. If a non-comment line is malformed,
has an unknown key, or has an empty value, the whole config is ignored and
defaults are used.

## Directions

### ToCodex

```powershell
.\bridge.ps1 -Direction ToCodex
```

Copies the selected production files from `WinCoordinatorDir` into the Codex
subset:

- Forge/core files copy flat into the Codex repo root.
- Lumen files copy flat into `Lumen\`.

After copying, the script asks:

```text
Enter a label if you want to commit and zip
```

- Blank input skips commit and zip.
- Non-blank input is used as the Git commit message.
- The script runs `git add .`, commits only if staged changes exist, pushes,
  then creates a zip under `TempDir`.

### ToWin

```powershell
.\bridge.ps1 -Direction ToWin
```

Copies files from the Codex subset back into their detailed paths under
`WinCoordinatorDir`.

- Forge/core files are read from the Codex repo root.
- Lumen files are read from `Lumen\`.
- Destination directories are created when needed.
- Missing Codex files are reported and skipped.
- No Git commit is made in the Win11 production repository.

## Bridge Scripts Usage Rule and Reasons

The bridge is an Archy-operated Windows/PowerShell tool. Codex may inspect,
edit, and reason about these scripts from WSL, but should not run bridge
directions by default.

Run a bridge direction only when Archy explicitly asks for that operation and
the current environment can actually run it. This WSL subset intentionally may
not have `pwsh`; that is fine. Do not invent a Linux workaround for bridge
execution.

The subset is intentionally smaller than the Win11 production project. This is
not only an ownership boundary; it is an operating constraint:

- the full production tree is much larger and includes many files that are not
  needed for the current reducer/projector/UI task;
- a smaller subset preserves Codex context window for reasoning, design, review,
  and collaboration instead of spending it on unrelated code volume;
- the subset gives Forge/Lumen the project gist while keeping patches focused
  and easy for Archy to bridge, inspect, compile, and test in the full project.

Risk profile:
- `ToCodex` overwrites selected files in this Codex subset from the Win11
  production repository, then may commit, push, and create a zip when Archy
  enters a label.
- `ToWin` overwrites selected files in the Win11 production repository.

When Codex changes bridge behavior, update this README and keep the `$Files` /
`$LumenFiles` lists aligned across bridge scripts.
