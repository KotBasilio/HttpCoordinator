$SrcDir  = "C:\Users\miron\source\repos\ImGuiCoo\ImGui-Coordinator"
$DestDir = "\\wsl.localhost\Ubuntu\home\miron\proj\HttpCoordinator"

$Files = @(
	"ui\utils\graph_types.h",
	"ui\utils\graph_types.cpp",
	"ui\utils\inspector_panel.cpp",
	"ui\utils\inspector_panel.h",
	"ui\utils\packets_ingestion.cpp",
	"ui\utils\packet_json_helpers.h",
	"ui\utils\packet_json_helpers.cpp",
	"ui\utils\users_ingestion.cpp",
	"ui\utils\presence_ingestion.cpp",
	"ui\utils\servers_ingestion.cpp",
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

# Store all labels here
$labels = @()

foreach ($file in $Files) {
    $srcPath  = Join-Path $SrcDir $file
    $destPath = $DestDir

    # Copy file (overwrite always)
    Copy-Item $srcPath $destPath -Force

    # Find first line with "#pragma message"
    $pragmaLine = Select-String -Path $srcPath -Pattern '#pragma message' | Select-Object -First 1

    if ($pragmaLine) {
        $msg = $pragmaLine.Line -replace '^\s*#pragma\s+message\s*\(?\s*"?', ''
        $msg = $msg -replace '"?\s*\)?\s*$', ''

        $revIndex = $msg.IndexOf("REV:")
        if ($revIndex -ge 0) {
            $msg = $msg.Substring($revIndex).Trim()
        }

        $labels += $msg
        Write-Host "$file -> $msg"
    } else {
        Write-Host "$file -> no stamp"
    }
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
		Make-Codebase-Zip -Files $Files -SrcDir $SrcDir -ZipName $mostFrequent.Name
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
