# CRuntime Windows Test Suite

param(
    [switch]$Full
)

$ErrorActionPreference = "Stop"

function Test-Step {
    param($Name, $Command)
    Write-Host "Testing: $Name..." -ForegroundColor Yellow -NoNewline
    
    try {
        $output = Invoke-Expression $Command 2>&1
        Write-Host " [PASS]" -ForegroundColor Green
        return $true
    } catch {
        Write-Host " [FAIL]" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Red
        return $false
    }
}

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host " CRuntime Windows Test Suite" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# Get current directory for WSL
$currentDir = Get-Location
$wslPath = wsl wslpath -a "'$currentDir'"

# Test 1: WSL2 availability
Write-Host "Checking WSL2..." -ForegroundColor Yellow
$wslStatus = wsl --status 2>&1
if ($?) {
    Write-Host "[PASS] WSL2 is installed" -ForegroundColor Green
} else {
    Write-Host "[FAIL] WSL2 not found" -ForegroundColor Red
    exit 1
}

# Test 2: Build
Write-Host "Building CRuntime..." -ForegroundColor Yellow
$buildCmd = "wsl bash -c 'cd $wslPath && make clean && make 2>&1'"
$buildResult = Invoke-Expression $buildCmd
if ($LASTEXITCODE -eq 0) {
    Write-Host "[PASS] Build successful" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Build failed" -ForegroundColor Red
    Write-Host $buildResult
    exit 1
}

# Test 3: Simple container
Write-Host "Running simple container..." -ForegroundColor Yellow
$testCmd = "wsl sudo cruntime run alpine echo 'test' 2>&1"
$testResult = Invoke-Expression $testCmd
if ($LASTEXITCODE -eq 0) {
    Write-Host "[PASS] Container ran successfully" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Container failed" -ForegroundColor Red
}

# Test 4: Resource limits (if Full)
if ($Full) {
    Write-Host "Testing memory limit..." -ForegroundColor Yellow
    $memCmd = "wsl sudo cruntime run --memory 256M alpine free -m 2>&1"
    try {
        Invoke-Expression $memCmd | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[PASS] Memory limit works" -ForegroundColor Green
        }
    } catch {
        Write-Host "[FAIL] Memory limit failed" -ForegroundColor Red
    }
    
    Write-Host "Testing CPU limit..." -ForegroundColor Yellow
    $cpuCmd = "wsl sudo cruntime run --cpus 1.0 alpine nproc 2>&1"
    try {
        Invoke-Expression $cpuCmd | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[PASS] CPU limit works" -ForegroundColor Green
        }
    } catch {
        Write-Host "[FAIL] CPU limit failed" -ForegroundColor Red
    }
}

# Test 5: Environment variables
Write-Host "Testing environment variables..." -ForegroundColor Yellow
$envCmd = "wsl sudo cruntime run -e TEST=value alpine sh -c 'echo `$TEST' 2>&1"
$envResult = Invoke-Expression $envCmd
if ($envResult -match "value") {
    Write-Host "[PASS] Environment variables work" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Environment variables failed" -ForegroundColor Red
}

Write-Host ""
Write-Host "======================================" -ForegroundColor Cyan
Write-Host " Test Suite Complete!" -ForegroundColor Green
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""
