# increment_build.ps1
# Pre-build script for NetPortChk.
# Reads src/version.h, increments VER_BUILD by 1, writes it back.
#
# Usage (from VS pre-build event):
#   powershell -ExecutionPolicy Bypass -File "$(ProjectDir)scripts\increment_build.ps1" -VersionFile "$(ProjectDir)src\version.h"

param(
    [Parameter(Mandatory=$true)]
    [string]$VersionFile
)

if (-not (Test-Path $VersionFile)) {
    Write-Error "ERROR: $VersionFile not found"
    exit 1
}

$content = Get-Content $VersionFile -Raw -Encoding UTF8

if ($content -notmatch '(#define\s+VER_BUILD\s+)(\d+)') {
    Write-Error "ERROR: VER_BUILD not found in $VersionFile"
    exit 1
}

$oldBuild = [int]$Matches[2]
$newBuild = $oldBuild + 1
$newContent = $content -replace '(#define\s+VER_BUILD\s+)\d+', "`${1}$newBuild"

Set-Content -Path $VersionFile -Value $newContent -Encoding UTF8 -NoNewline

Write-Host "VER_BUILD: $oldBuild -> $newBuild"
