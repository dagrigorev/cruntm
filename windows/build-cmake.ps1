# CMake Build Script for WSL2

param(
    [switch]$Clean,
    [switch]$Debug,
    [switch]$Install,
    [string]$Generator = "Unix Makefiles"
)

$ErrorActionPreference = "Stop"

Write-Host "CRuntime CMake Build (WSL2)" -ForegroundColor Cyan
Write-Host ""

# Get WSL path
$windowsPath = (Get-Location).Path
$wslPath = wsl wslpath -a "'$windowsPath'"

if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    wsl bash -c "cd '$wslPath' && rm -rf build"
    Write-Host "[OK] Clean complete" -ForegroundColor Green
    exit 0
}

# Create build directory
wsl bash -c "cd '$wslPath' && mkdir -p build"

# Configure CMake
Write-Host "Configuring CMake..." -ForegroundColor Yellow

$buildType = if ($Debug) { "Debug" } else { "Release" }

$configScript = "cd '$wslPath/build' && cmake .. -G '$Generator' -DCMAKE_BUILD_TYPE=$buildType"
wsl bash -c $configScript

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] CMake configuration failed" -ForegroundColor Red
    exit 1
}

# Build
Write-Host "Building..." -ForegroundColor Yellow

$buildScript = "cd '$wslPath/build' && cmake --build . --config $buildType -j `$(nproc)"
wsl bash -c $buildScript

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Build failed" -ForegroundColor Red
    exit 1
}

Write-Host "[OK] Build successful" -ForegroundColor Green

# Install
if ($Install) {
    Write-Host "Installing..." -ForegroundColor Yellow
    
    $installScript = "cd '$wslPath/build' && sudo cmake --install ."
    wsl bash -c $installScript
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[OK] Installation complete" -ForegroundColor Green
    } else {
        Write-Host "[ERROR] Installation failed" -ForegroundColor Red
        exit 1
    }
}

Write-Host ""
Write-Host "[OK] Done!" -ForegroundColor Green
