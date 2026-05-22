param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("ToCodex", "ToWin")]
    [string]$Direction
)

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
#   "ui\utils\stubs.cpp",
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

function Assert-NoDuplicateFlatBasenames {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Files
    )

    $dupes = @(
        $Files |
            ForEach-Object { Split-Path $_ -Leaf } |
            Group-Object |
            Where-Object { $_.Count -gt 1 }
    )

    if ($dupes.Count -eq 0) {
        return
    }

    Write-Host "Ambiguous flat-file copy: duplicate basenames detected:" -ForegroundColor Red
    foreach ($d in $dupes) {
        Write-Host "  $($d.Name) appears $($d.Count) times"
    }
    throw "Cannot safely copy from flat CodexDir with duplicate basenames."
}

function Copy-ToCodexFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelPath,

        [Parameter(Mandatory = $true)]
        [string]$WinDir,

        [Parameter(Mandatory = $true)]
        [string]$DestRoot
    )

    $srcPath = Join-Path $WinDir $RelPath
    Copy-Item $srcPath $DestRoot -Force
    Write-Host "$RelPath -> $DestRoot"
}

function Copy-ToWinFile {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RelPath,

        [Parameter(Mandatory = $true)]
        [string]$SourceRoot,

        [Parameter(Mandatory = $true)]
        [string]$WinDir,

        [switch]$Lumen
    )

    $fileName = Split-Path $RelPath -Leaf
    $srcPath = Join-Path $SourceRoot $fileName
    $dstPath = Join-Path $WinDir $RelPath
    $dstDir = Split-Path $dstPath -Parent

    $displaySource = if ($Lumen) { "Lumen/$fileName" } else { $fileName }

    if (-not (Test-Path $srcPath)) {
        Write-Host "MISSING: $displaySource  (expected at $srcPath)" -ForegroundColor Yellow
        $script:missing++
        return
    }

    if (-not (Test-Path $dstDir)) {
        New-Item -ItemType Directory -Path $dstDir -Force | Out-Null
    }

    Copy-Item $srcPath $dstPath -Force
    $script:copied++
    Write-Host "$displaySource -> $RelPath"
}

function Invoke-ToCodexBridge {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Paths
    )

    $WinDir = $Paths.WinCoordinatorDir
    $CodexDir = $Paths.CodexCoordinatorDir
    Write-Host "Bridge Win -> CODEX"
    Write-Host "WinDir:   $WinDir"
    Write-Host "CodexDir: $CodexDir"
    Write-Host ""

    if (-not (Test-Path $WinDir)) {
        throw "WinDir not found: $WinDir"
    }

    if (-not (Test-Path $CodexDir)) {
        New-Item -ItemType Directory -Path $CodexDir | Out-Null
    }

    $LumenDestDir = Join-Path $CodexDir "Lumen"
    if (-not (Test-Path $LumenDestDir)) {
        New-Item -ItemType Directory -Path $LumenDestDir | Out-Null
    }

    foreach ($file in $Files) {
        Copy-ToCodexFile -RelPath $file -WinDir $WinDir -DestRoot $CodexDir
    }

    foreach ($file in $LumenFiles) {
        Copy-ToCodexFile -RelPath $file -WinDir $WinDir -DestRoot $LumenDestDir
    }
}

function Invoke-ToWinBridge {
    param(
        [Parameter(Mandatory = $true)]
        [pscustomobject]$Paths
    )

    $WinDir = $Paths.WinCoordinatorDir
    $CodexDir = $Paths.CodexCoordinatorDir

    Write-Host "Bridge CODEX -> Win"
    Write-Host "CodexDir: $CodexDir"
    Write-Host "WinDir:   $WinDir"
    Write-Host ""

    if (-not (Test-Path $CodexDir)) {
        throw "CodexDir not found: $CodexDir"
    }

    if (-not (Test-Path $WinDir)) {
        throw "WinDir not found: $WinDir"
    }

    Assert-NoDuplicateFlatBasenames -Files $Files

    $script:copied = 0
    $script:missing = 0

    foreach ($file in $Files) {
        Copy-ToWinFile -RelPath $file -SourceRoot $CodexDir -WinDir $WinDir
    }

    $lumenSourceRoot = Join-Path $CodexDir "Lumen"
    foreach ($file in $LumenFiles) {
        Copy-ToWinFile -RelPath $file -SourceRoot $lumenSourceRoot -WinDir $WinDir -Lumen
    }

    Write-Host ""
    Write-Host "Copied $script:copied file(s). Missing $script:missing file(s)."
}

$BridgePaths = Get-BridgePaths

switch ($Direction) {
    "ToCodex" { Invoke-ToCodexBridge -Paths $BridgePaths }
    "ToWin"   { Invoke-ToWinBridge -Paths $BridgePaths }
}
