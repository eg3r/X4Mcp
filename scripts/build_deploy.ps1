<# .SYNOPSIS
    Build and deploy x4mcp to the game directory.
.DESCRIPTION
    Configures (if needed), builds, and deploys x4mcp in one step.
    Defaults to Debug config. Use -Release for Release builds.
.EXAMPLE
    .\scripts\build_deploy.ps1
    .\scripts\build_deploy.ps1 -Release
    .\scripts\build_deploy.ps1 -Config RelWithDebInfo
    .\scripts\build_deploy.ps1 -Clean
#>
param(
    [string]$Config = "Debug",
    [switch]$Release,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

if ($Release) { $Config = "Release" }

# Ensure CMake is on PATH (VS2022 BuildTools bundled CMake)
$vsRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
$cmakeBin = "$vsRoot\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if (Test-Path $cmakeBin) {
    $env:PATH = "$cmakeBin;$env:PATH"
}

$root = Split-Path $PSScriptRoot -Parent
$build = Join-Path $root "build"

Write-Host "=== x4mcp build & deploy ($Config) ===" -ForegroundColor Cyan

# Clean if requested
if ($Clean -and (Test-Path $build)) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item $build -Recurse -Force
}

# Configure (only if needed)
if (-not (Test-Path (Join-Path $build "CMakeCache.txt"))) {
    Write-Host "`n--- Configure ---" -ForegroundColor Green
    Push-Location $root
    cmake -B build -G "Visual Studio 17 2022" -A x64
    Pop-Location
    if ($LASTEXITCODE -ne 0) { Write-Error "Configure failed"; exit 1 }
}

# Build
Write-Host "`n--- Build ($Config) ---" -ForegroundColor Green
cmake --build $build --config $Config
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

# Deploy (CMake deploy target -> X4_GAME_DIR)
Write-Host "`n--- Deploy ---" -ForegroundColor Green
cmake --build $build --config $Config --target deploy
if ($LASTEXITCODE -ne 0) { Write-Error "Deploy failed"; exit 1 }

Write-Host "`n=== Done ===" -ForegroundColor Cyan
