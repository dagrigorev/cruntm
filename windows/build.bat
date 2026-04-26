@echo off
REM CRuntime Windows Build Wrapper

SET SCRIPT_DIR=%~dp0

echo ======================================
echo  CRuntime Windows Build
echo ======================================
echo.

REM Check if PowerShell is available
where powershell >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo ERROR: PowerShell is not available
    exit /b 1
)

REM Run PowerShell script
powershell -ExecutionPolicy Bypass -File "%SCRIPT_DIR%build.ps1" %*

if %ERRORLEVEL% equ 0 (
    echo.
    echo Build completed successfully!
    echo.
    echo Run containers with:
    echo   wsl sudo cruntime run alpine /bin/sh
    echo.
) else (
    echo.
    echo Build failed!
    exit /b 1
)
