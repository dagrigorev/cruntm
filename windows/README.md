# Windows Quick Start

## Prerequisites

- Windows 10 (version 2004+) or Windows 11
- Administrator access

## 1-Minute Setup

### Install WSL2

Open PowerShell as Administrator:

```powershell
wsl --install
```

Reboot when prompted.

### Build CRuntime

After reboot, open PowerShell:

```powershell
cd path\to\cruntime
.\windows\build.ps1 -Install -Test
```

Done! The script will:
- ✓ Install Ubuntu in WSL2
- ✓ Install build dependencies
- ✓ Build CRuntime
- ✓ Install to /usr/local/bin
- ✓ Run a test container

## Run Your First Container

```powershell
wsl sudo cruntime run alpine echo "Hello from container!"
```

## Development Workflow

### Option 1: VS Code (Recommended)

```powershell
# Install VS Code extensions
code --install-extension ms-vscode-remote.remote-wsl
code --install-extension ms-vscode.cpptools

# Open project in WSL
code --remote wsl+Ubuntu .
```

Now you can:
- Edit files in Windows
- Build with Ctrl+Shift+B
- Debug with F5
- IntelliSense works automatically

### Option 2: Visual Studio 2022

1. Open Visual Studio 2022
2. File → Open → CMake → Select CMakeLists.txt
3. Select "WSL-GCC-Debug" or "WSL-GCC-Release" configuration
4. Build and debug directly

### Option 3: Command Line

```powershell
# Build
.\windows\build.ps1

# Clean
.\windows\build.ps1 -Clean

# Install
.\windows\build.ps1 -Install

# Test
.\windows\build.ps1 -Test
```

## Common Commands

```powershell
# Interactive shell
wsl sudo cruntime run alpine /bin/sh

# With resource limits
wsl sudo cruntime run --memory 512M --cpus 1.0 alpine /bin/sh

# Port forwarding (accessible from Windows)
wsl sudo cruntime run -p 8080:80 alpine httpd
# Open browser: http://localhost:8080

# Mount Windows directory
wsl sudo cruntime run -v /mnt/c/data:/data alpine ls /data
```

## Troubleshooting

### "WSL is not installed"

```powershell
# Enable WSL feature
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart

# Reboot
shutdown /r /t 0

# After reboot
wsl --install
```

### "Build failed"

```powershell
# Update WSL kernel
wsl --update

# Reinstall dependencies
wsl bash -c "sudo apt-get update && sudo apt-get install -y build-essential"

# Rebuild
.\windows\build.ps1 -Clean
.\windows\build.ps1
```

### "Permission denied"

```powershell
# Always use 'sudo' in WSL
wsl sudo cruntime run alpine /bin/sh

# Or become root
wsl
sudo su -
cruntime run alpine /bin/sh
```

## Next Steps

- Read [WINDOWS_GUIDE.md](WINDOWS_GUIDE.md) for detailed Windows integration
- Read [../QUICKSTART.md](../QUICKSTART.md) for basic usage
- Read [../README.md](../README.md) for complete documentation
- Check [../examples/](../examples/) for sample configurations

## File Access

Your Windows C: drive is accessible at `/mnt/c/` in WSL:

```powershell
# Windows: C:\Users\YourName\data
# WSL: /mnt/c/Users/YourName/data

# Mount in container
wsl sudo cruntime run -v /mnt/c/Users/YourName/data:/data alpine ls /data
```

From Windows, access WSL files via:
```
\\wsl$\Ubuntu\home\username\cruntime
```

## Performance Tips

Store project files in WSL filesystem for best performance:

```bash
# In WSL
cd ~
tar -xzf /mnt/c/Users/YourName/Downloads/cruntime.tar.gz
cd cruntime
make
```

## Support

- WSL Issues: https://docs.microsoft.com/windows/wsl/troubleshooting
- CRuntime Issues: See main README.md
