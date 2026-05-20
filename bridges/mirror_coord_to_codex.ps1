$SrcDir  = "C:\Users\miron\source\repos\ImGuiCoo\ImGui-Coordinator"
$DestDir = "\\wsl.localhost\Ubuntu\home\miron\proj\HttpCoordinator"

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
	"ui\ui\utils\texture_manager.cpp",
	"ui\ui\utils\texture_manager.h",
	"ui\utils\AssetsEnum.h",
	"Assets\gen_assets_enum.py"
)

function Make-Codebase-Zip {
    param (
        [string[]]$Files,
        [string]$SrcDir,
        [string]$ZipName
    )

    $tmpDir = "D:\tmp\Codebase"

    # Clear folder
    if (Test-Path $tmpDir) {
        Remove-Item $tmpDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $tmpDir | Out-Null

    # Copy files flat
    foreach ($file in $Files) {
        $srcPath = Join-Path $SrcDir $file
        $destPath = Join-Path $tmpDir (Split-Path $file -Leaf)
        Copy-Item $srcPath $destPath -Force
    }

    # Adjust name: replace "REV:" with "Codebase-", remove spaces
    $zipBaseName = ($ZipName -replace '^\s*REV:\s*', 'Codebase-') -replace '\s+', ''
    $zipPath = "D:\tmp\$zipBaseName.zip"

    # Zip with name (no spaces)
    if (Test-Path $zipPath) {
        Remove-Item $zipPath -Force
    }
    Compress-Archive -Path "$tmpDir\*" -DestinationPath $zipPath

    Write-Host "Codebase zipped to $zipPath"
}

# Ensure destination exists
if (-not (Test-Path $DestDir)) {
    New-Item -ItemType Directory -Path $DestDir | Out-Null
}

$LumenDestDir = Join-Path $DestDir "Lumen"
if (-not (Test-Path $LumenDestDir)) {
    New-Item -ItemType Directory -Path $LumenDestDir | Out-Null
}

function Get-RevLabel {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $pragmaLine = Select-String -Path $Path -Pattern '#pragma message' | Select-Object -First 1
    if (-not $pragmaLine) {
        return $null
    }

    $msg = $pragmaLine.Line -replace '^\s*#pragma\s+message\s*\(?\s*"?', ''
    $msg = $msg -replace '"?\s*\)?\s*$', ''

    $revIndex = $msg.IndexOf("REV:")
    if ($revIndex -ge 0) {
        $msg = $msg.Substring($revIndex).Trim()
    }

    return $msg
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

    $label = Get-RevLabel -Path $srcPath
    if ($label) {
        $script:labels += $label
        Write-Host "$RelPath -> $label"
    } else {
        Write-Host "$RelPath -> no stamp"
    }
}

# Store all labels here
$labels = @()

foreach ($file in $Files) {
    Copy-MirroredFile -RelPath $file -DestRoot $DestDir
}

foreach ($file in $LumenFiles) {
    Copy-MirroredFile -RelPath $file -DestRoot $LumenDestDir
}

# === After all files copied, detect most frequent label ===
if ($labels.Count -gt 0) {
	$mostFrequent = $labels | Group-Object | Sort-Object Count -Descending | Select-Object -First 1
	Write-Host "Most frequent label: $($mostFrequent.Name) (appeared $($mostFrequent.Count) times)"

	# Ask user before committing
	$commitMsg = $mostFrequent.Name
	$answer = Read-Host "Do you want to commit with this label? (y/n)"
	if ($answer -imatch '^(n)$') {
		Write-Host "Commit skipped."
		Make-Codebase-Zip -Files ($Files + $LumenFiles) -SrcDir $SrcDir -ZipName $mostFrequent.Name
	} else {
		if ([string]::IsNullOrWhiteSpace($answer) -or $answer -imatch '^(y|yes)$') {
			# keep $commitMsg as-is
		} else {
			$commitMsg = $answer
		}
		# Step into destination directory and commit
		Push-Location $DestDir
		git add .
		git commit -m "$commitMsg"
		git push
		Pop-Location
	}

} else {
	Write-Host "No labels detected in any file."
}
