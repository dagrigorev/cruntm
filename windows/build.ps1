# CRuntime Windows Build Script (WSL2)
# Builds and runs the container runtime in WSL2

param(
    [switch]$Install,
    [switch]$Clean,
    [switch]$Test,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Write-ColorOutput($ForegroundColor) {
    $fc = $host.UI.RawUI.ForegroundColor
    $host.UI.RawUI.ForegroundColor = $ForegroundColor
    if ($args) {
        Write-Output $args
    }
    $host.UI.RawUI.ForegroundColor = $fc
}

function Check-WSL2 {
    Write-ColorOutput Yellow "Checking WSL2..."
    
    if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) {
        Write-ColorOutput Red "ERROR: WSL is not installed"
        Write-Host "Install WSL2 with: wsl --install"
        exit 1
    }
    
    $wslVersion = wsl --status 2>&1 | Select-String "Default Version" | ForEach-Object { $_.ToString().Split(":")[1].Trim() }
    
    if ($wslVersion -ne "2") {
        Write-ColorOutput Yellow "WARNING: Default WSL version is not 2"
        Write-Host "Set default to WSL2: wsl --set-default-version 2"
    }
    
    Write-ColorOutput Green "[OK] WSL2 is available"
}

function Check-Distribution {
    Write-ColorOutput Yellow "Checking WSL distribution..."
    
    $distros = wsl --list --quiet
    
    if ($distros.Count -eq 0) {
        Write-ColorOutput Red "ERROR: No WSL distributions installed"
        Write-Host "Install Ubuntu: wsl --install -d Ubuntu"
        exit 1
    }
    
    $defaultDistro = wsl --list --quiet | Select-Object -First 1
    Write-ColorOutput Green "[OK] Using distribution: $defaultDistro"
}

function Install-Dependencies {
    Write-ColorOutput Yellow "Installing build dependencies in WSL..."
    
    $script = @"
sudo apt-get update -qq
sudo apt-get install -y build-essential iproute2 iptables util-linux wget
echo 'Dependencies installed successfully'
"@
    
    wsl bash -c $script
    
    if ($LASTEXITCODE -eq 0) {
        Write-ColorOutput Green "[OK] Dependencies installed"
    } else {
        Write-ColorOutput Red "[ERROR] Failed to install dependencies"
        exit 1
    }
}

function Build-CRuntime {
    Write-ColorOutput Yellow "Building CRuntime in WSL..."
    
    # Get Windows path and convert to WSL path
    $windowsPath = (Get-Location).Path
    $wslPath = wsl wslpath -a "'$windowsPath'"
    
    $script = @"
cd '$wslPath'
make clean
make
echo 'Build completed successfully'
"@
    
    wsl bash -c $script
    
    if ($LASTEXITCODE -eq 0) {
        Write-ColorOutput Green "[OK] Build successful"
    } else {
        Write-ColorOutput Red "[ERROR] Build failed"
        exit 1
    }
}

function Install-CRuntime {
    Write-ColorOutput Yellow "Installing CRuntime in WSL..."
    
    $windowsPath = (Get-Location).Path
    $wslPath = wsl wslpath -a "'$windowsPath'"
    
    $script = @"
cd '$wslPath'
sudo make install
echo 'Installation completed successfully'
"@
    
    wsl bash -c $script
    
    if ($LASTEXITCODE -eq 0) {
        Write-ColorOutput Green "[OK] Installed to /usr/local/bin/cruntime"
    } else {
        Write-ColorOutput Red "[ERROR] Installation failed"
        exit 1
    }
}

function Test-CRuntime {
    Write-ColorOutput Yellow "Running test container..."
    
    $windowsPath = (Get-Location).Path
    $wslPath = wsl wslpath -a "'$windowsPath'"
    
    $script = @"
cd '$wslPath'
sudo ./setup.sh
"@
    
    wsl bash -c $script
    
    if ($LASTEXITCODE -eq 0) {
        Write-ColorOutput Green "[OK] Test successful"
    } else {
        Write-ColorOutput Red "[ERROR] Test failed"
    }
}

function Clean-Build {
    Write-ColorOutput Yellow "Cleaning build artifacts..."
    
    $windowsPath = (Get-Location).Path
    $wslPath = wsl wslpath -a "'$windowsPath'"
    
    $script = @"
cd '$wslPath'
make clean
"@
    
    wsl bash -c $script
    
    Write-ColorOutput Green "[OK] Clean complete"
}

function Show-Help {
    Write-Host "CRuntime Windows Build Script"
    Write-Host ""
    Write-Host "Usage: .\build.ps1 [options]"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "    -Install    Build and install CRuntime"
    Write-Host "    -Clean      Clean build artifacts"
    Write-Host "    -Test       Run test container"
    Write-Host "    -Help       Show this help"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "    .\build.ps1                 # Build only"
    Write-Host "    .\build.ps1 -Install        # Build and install"
    Write-Host "    .\build.ps1 -Test           # Build and test"
    Write-Host "    .\build.ps1 -Clean          # Clean build"
    Write-Host ""
    Write-Host "After installation, run containers with:"
    Write-Host "    wsl sudo cruntime run alpine /bin/sh"
    Write-Host ""
}

# Main execution
Write-Host ""
Write-ColorOutput Cyan "======================================"
Write-ColorOutput Cyan " CRuntime Windows Build (WSL2)"
Write-ColorOutput Cyan "======================================"
Write-Host ""

if ($Help) {
    Show-Help
    exit 0
}

# Check prerequisites
Check-WSL2
Check-Distribution

if ($Clean) {
    Clean-Build
    exit 0
}

# Install dependencies
Install-Dependencies

# Build
Build-CRuntime

# Install if requested
if ($Install) {
    Install-CRuntime
}

# Test if requested
if ($Test) {
    Test-CRuntime
}

Write-Host ""
Write-ColorOutput Green "======================================"
Write-ColorOutput Green " Build Complete!"
Write-ColorOutput Green "======================================"
Write-Host ""
Write-Host "Run containers with:"
Write-Host "  wsl sudo cruntime run alpine /bin/sh"
Write-Host ""
Write-Host "Or enter WSL:"
Write-Host "  wsl"
Write-Host "  sudo cruntime run alpine /bin/sh"
Write-Host ""
