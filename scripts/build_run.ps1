<# .SYNOPSIS
    Build, deploy, and launch X4 Foundations with x4mcp.
.DESCRIPTION
    Runs build_deploy.ps1, then launches X4.exe if it isn't already running.
    If X4 is already running, skips launch (autoreload via /reloadui picks up changes).
.EXAMPLE
    .\scripts\build_run.ps1
    .\scripts\build_run.ps1 -Release
    .\scripts\build_run.ps1 -SkipBuild
#>
param(
    [string]$Config = "Debug",
    [switch]$Release,
    [switch]$Clean,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Continue"

if ($Release) { $Config = "Release" }

# Build & deploy
if (-not $SkipBuild) {
    $buildArgs = @{ Config = $Config }
    if ($Clean) { $buildArgs.Clean = $true }
    & "$PSScriptRoot\build_deploy.ps1" @buildArgs
    if ($LASTEXITCODE -ne 0) { exit 1 }
}

# Locate X4.exe via CMakeCache
$projectRoot = Split-Path $PSScriptRoot -Parent
$cacheFile = "$projectRoot\build\CMakeCache.txt"
$gameDir = [string]::Empty
if (Test-Path $cacheFile) {
    foreach ($ln in Get-Content $cacheFile) {
        if ($ln -match "^X4_GAME_DIR:PATH=(.+)$") {
            $gameDir = $Matches[1].Replace("/", "\").Trim()
            break
        }
    }
}

if ([string]::IsNullOrWhiteSpace($gameDir)) {
    Write-Error "Cannot find X4 game directory -- set X4_GAME_DIR in CMake or launch manually."
    exit 1
}

$x4exe = "$gameDir\X4.exe"
if (-not (Test-Path $x4exe)) {
    Write-Error "Cannot find X4.exe at $x4exe"
    exit 1
}

# Check if X4 is already running (by path)
$running = Get-Process -Name "X4" -ErrorAction SilentlyContinue
foreach ($p in @($running)) {
    if ($p.Path -and $p.Path.StartsWith($gameDir, [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Host "`nX4 already running (PID $($p.Id)) -- use /reloadui in-game to pick up changes." -ForegroundColor Yellow
        exit 0
    }
}

Write-Host "`n--- Launching X4 ---" -ForegroundColor Green
Start-Process -FilePath $x4exe -ArgumentList "-debug", "scripts", "-logfile", "debuglog.txt", "-skipintro" -WorkingDirectory $gameDir
Write-Host "X4 started from $gameDir" -ForegroundColor Cyan
