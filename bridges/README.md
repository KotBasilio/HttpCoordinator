# Bridge Scripts

These scripts are Archy-operated helpers for moving the small Codex WSL subset
to and from the full Win11 Coordinator repository.

Preferred Windows / PowerShell entry point:

```powershell
.\bridge.ps1 -Direction ToCodex
.\bridge.ps1 -Direction ToWin
```

Approved WSL / Bash entry point:

```bash
./linbridge ToCodex
./linbridge ToWin
./linbridge -Direction ToCodex
./linbridge -Direction ToWin
```

`linbridge` exists for running the bridge from WSL when PowerShell warns on the
`\\wsl.localhost` script path. It is tracked as executable; if that bit is ever
missing after a copy, restore it from WSL with `chmod +x bridges/linbridge`.

## Paths

`bridge.ps1` and `linbridge` have built-in defaults, then optionally read:

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

The bridge is an Archy-operated helper. Codex may inspect, edit, and reason
about these scripts from WSL, but should not run bridge directions by default
unless Archy explicitly asks.

`bridge.ps1` is the Windows/PowerShell entry point. `linbridge` is the approved
WSL/Bash entry point, added to avoid PowerShell trust prompts on
`\\wsl.localhost` script paths. Do not invent additional bridge execution
workarounds unless Archy asks.

The bridged subset is intentionally smaller than the Win11 production project. This is
not only an ownership boundary; it is an operating constraint:

- the full production tree is much larger and includes many files that are not
  needed for the current reducer/projector/UI task;
- a smaller subset preserves Codex context window for reasoning, design, review,
  and collaboration instead of spending it on unrelated code volume;
- the subset gives Forge/Lumen the project gist while keeping patches focused
  and easy for Archy to bridge, inspect, compile, and test in the full project.

When Codex changes bridge behavior, update this README and keep the `$Files` /
`$LumenFiles` lists aligned across bridge scripts.
