# grab_codex_to_coord.ps1
# One-way bridge:
#   CODEX flat repo -> Windows Coordinator repo detailed paths
#
# Copies files by basename from $CodexDir into their detailed relative paths under $WinDir.
# Does NOT commit.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-BridgePaths {
    param(
        [string]$ConfigPath = "D:\miron\wip\codex_paths_config.txt"
    )

    $paths = [ordered]@{
        WinCoordinatorDir   = "C:\Users\miron\source\repos\ImGuiCoo\ImGui-Coordinator"
        CodexCoordinatorDir = "\\wsl.localhost\Ubuntu\home\miron\proj\HttpCoordinator"
        TempDir             = "D:\tmp"
    }

    if (-not (Test-Path $ConfigPath)) {
        Write-Host "Bridge path config not found; using defaults: $ConfigPath"
        return [pscustomobject]$paths
    }

    $overrides = @{}
    $validConfig = $true
    $lineNo = 0

    foreach ($rawLine in Get-Content $ConfigPath) {
        $lineNo++
        $line = $rawLine.Trim()
        if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
            continue
        }

        $parts = $line -split "=", 2
        if ($parts.Count -ne 2) {
            Write-Host "Ignoring bridge path config; malformed line ${lineNo}: $rawLine" -ForegroundColor Yellow
            $validConfig = $false
            break
        }

        $key = $parts[0].Trim()
        $value = $parts[1].Trim()

        if (-not $paths.Contains($key)) {
            Write-Host "Ignoring bridge path config; unknown key on line ${lineNo}: $key" -ForegroundColor Yellow
            $validConfig = $false
            break
        }

        if ([string]::IsNullOrWhiteSpace($value)) {
            Write-Host "Ignoring bridge path config; empty value on line ${lineNo}: $key" -ForegroundColor Yellow
            $validConfig = $false
            break
        }

        $overrides[$key] = $value
    }

    if ($validConfig) {
        foreach ($key in $overrides.Keys) {
            $paths[$key] = $overrides[$key]
        }

        if ($overrides.Count -gt 0) {
            Write-Host "Bridge path config loaded: $ConfigPath"
        }
    } else {
        Write-Host "Using built-in bridge path defaults." -ForegroundColor Yellow
    }

    return [pscustomobject]$paths
}

$BridgePaths = Get-BridgePaths
$WinDir    = $BridgePaths.WinCoordinatorDir
$CodexDir  = $BridgePaths.CodexCoordinatorDir

# Keep the SAME $Files and $LumenFiles arrays from mirror_coord_to_codex.ps1 here.
$Files = @(
	"ui\utils\graph_types.h",
	"ui\utils\graph_types.cpp",
	"ui\utils\inspector_panel.cpp",
	"ui\utils\inspector_panel.h",
	"ui\ingest\packets_ingestion.cpp",
	"ui\ingest\packet_json_helpers.h",
	"ui\ingest\packet_json_helpers.cpp",
	"ui\ingest\users_ingestion.cpp",
	"ui\ingest\presence_ingestion.cpp",
	"ui\ingest\servers_ingestion.cpp",
	"ui\proj\projector.cpp",
	"ui\proj\projector_internal.h",
	"ui\proj\projector_keys_values.cpp",
	"ui\proj\projector_layout.cpp",
	"ui\proj\projector_links.cpp",
	"ui\proj\projector_nodes.cpp",
	"ui\utils\coordinator_http_server.cpp",
#	"ui\utils\stubs.cpp",
	"ui\controllers\live_state.cpp",
	"ui\controllers\live_state.h",
	"ui\models\ReqNamesEnum.h",
	"ui\controllers\coordinator_http_server.h",
	"ui\controllers\start_server_controller.cpp",
	"ui\controllers\start_server_controller.h",
	"ui\models\main_model.h"
)

$LumenFiles = @(
	"ui\utils\texture_manager.cpp",
	"ui\utils\texture_manager.h",
	"ui\utils\AssetsEnum.h",
	"ui\utils\zoom_control_widget.cpp",
	"ui\utils\graph_panel.h",
	"ui\utils\graph_panel.cpp",
	"Assets\gen_assets_enum.py"
)

function Get-RevLabel {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $m = Select-String -Path $Path -Pattern '#pragma\s+message' | Select-Object -First 1
    if (-not $m) {
        return $null
    }

    $line = $m.Line

    # Prefer extracting from REV: onward.
    $revIdx = $line.IndexOf("REV:")
    if ($revIdx -ge 0) {
        $label = $line.Substring($revIdx).Trim()

        # Remove trailing quote/paren noise if present.
        $label = $label -replace '[\"\)\s]+$', ''
        return $label
    }

    # Fallback: return pragma message line itself, trimmed.
    return $line.Trim()
}

Write-Host "Grab CODEX -> Coordinator"
Write-Host "CodexDir: $CodexDir"
Write-Host "WinDir:   $WinDir"
Write-Host ""

if (-not (Test-Path $CodexDir)) {
    throw "CodexDir not found: $CodexDir"
}

if (-not (Test-Path $WinDir)) {
    throw "WinDir not found: $WinDir"
}

# Since CodexDir is flat, duplicate basenames would be ambiguous.
$dupes = @(
    $Files |
        ForEach-Object { Split-Path $_ -Leaf } |
        Group-Object |
        Where-Object { $_.Count -gt 1 }
)

if ($dupes.Count -gt 0) {
    Write-Host "Ambiguous flat-file copy: duplicate basenames detected:" -ForegroundColor Red
    foreach ($d in $dupes) {
        Write-Host "  $($d.Name) appears $($d.Count) times"
    }
    throw "Cannot safely copy from flat CodexDir with duplicate basenames."
}

$labels = @()
$copied = 0
$missing = 0

foreach ($relPath in $Files) {
    $fileName = Split-Path $relPath -Leaf

    # Source is flat in CodexDir.
    $srcPath = Join-Path $CodexDir $fileName

    # Destination preserves detailed path in Windows repo.
    $dstPath = Join-Path $WinDir $relPath
    $dstDir  = Split-Path $dstPath -Parent

    if (-not (Test-Path $srcPath)) {
        Write-Host "MISSING: $fileName  (expected at $srcPath)" -ForegroundColor Yellow
        $missing++
        continue
    }

    if (-not (Test-Path $dstDir)) {
        New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    }

    Copy-Item $srcPath $dstPath -Force
    $copied++

    $label = Get-RevLabel -Path $srcPath
    if ($label) {
        $labels += $label
        Write-Host "$fileName -> $relPath  [$label]"
    } else {
        Write-Host "$fileName -> $relPath  [no REV label]"
    }
}

foreach ($relPath in $LumenFiles) {
    $fileName = Split-Path $relPath -Leaf

    # Lumen-owned files live under the Lumen lane folder in Codex.
    $srcPath = Join-Path (Join-Path $CodexDir "Lumen") $fileName

    # Destination preserves detailed path in Windows repo.
    $dstPath = Join-Path $WinDir $relPath
    $dstDir  = Split-Path $dstPath -Parent

    if (-not (Test-Path $srcPath)) {
        Write-Host "MISSING: Lumen/$fileName  (expected at $srcPath)" -ForegroundColor Yellow
        $missing++
        continue
    }

    if (-not (Test-Path $dstDir)) {
        New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    }

    Copy-Item $srcPath $dstPath -Force
    $copied++

    $label = Get-RevLabel -Path $srcPath
    if ($label) {
        $labels += $label
        Write-Host "Lumen/$fileName -> $relPath  [$label]"
    } else {
        Write-Host "Lumen/$fileName -> $relPath  [no REV label]"
    }
}

Write-Host ""
Write-Host "Copied $copied file(s). Missing $missing file(s)."
Write-Host ""

if ($labels.Count -gt 0) {
	$mostFrequent = $labels | Group-Object | Sort-Object Count -Descending | Select-Object -First 1
	Write-Host "Grabbed from CODEX"
	Write-Host "Most frequent label: $($mostFrequent.Name) (appeared $($mostFrequent.Count) times)"

	$uniqueLabels = @($labels | Sort-Object -Unique)

	if ($uniqueLabels.Count -gt 1) {
		Write-Host ""
		Write-Host "All detected labels:" -ForegroundColor Yellow
		foreach ($l in $uniqueLabels) {
			$count = @($labels | Where-Object { $_ -eq $l }).Count
			Write-Host "  $l ($count)"
		}
	}
} else {
    Write-Host "No labels detected in any file."
}
