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
$SrcDir  = $BridgePaths.WinCoordinatorDir
$DestDir = $BridgePaths.CodexCoordinatorDir

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
	"ui\utils\texman_lods.cpp",
	"ui\utils\texture_manager.h",
	"ui\utils\AssetsEnum.h",
	"ui\utils\zoom_control_widget.cpp",
	"ui\utils\graph_panel.h",
	"ui\utils\graph_panel.cpp",
	"Assets\gen_assets_enum.py"
)

# Ensure destination exists
if (-not (Test-Path $DestDir)) {
    New-Item -ItemType Directory -Path $DestDir | Out-Null
}

$LumenDestDir = Join-Path $DestDir "Lumen"
if (-not (Test-Path $LumenDestDir)) {
    New-Item -ItemType Directory -Path $LumenDestDir | Out-Null
}

function Copy-MirroredFile {
    param (
        [Parameter(Mandatory = $true)]
        [string]$RelPath,

        [Parameter(Mandatory = $true)]
        [string]$DestRoot
    )

    $srcPath = Join-Path $SrcDir $RelPath

    # Copy files flat into the selected Codex destination folder.
    Copy-Item $srcPath $DestRoot -Force
    Write-Host "$RelPath -> $DestRoot"
}

foreach ($file in $Files) {
    Copy-MirroredFile -RelPath $file -DestRoot $DestDir
}

foreach ($file in $LumenFiles) {
    Copy-MirroredFile -RelPath $file -DestRoot $LumenDestDir
}
