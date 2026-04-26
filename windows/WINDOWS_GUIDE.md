# CRuntime on Windows

This guide covers three methods to build and run CRuntime on Windows.

## Method 1: WSL2 (Recommended)

**Use Linux containers directly in Windows with full functionality.**

### Prerequisites

- Windows 10 version 2004+ or Windows 11
- WSL2 enabled
- Ubuntu (or any Linux distribution) installed in WSL

### Quick Setup

#### 1. Enable WSL2 (if not already enabled)

Open PowerShell as Administrator:

```powershell
# Enable WSL
wsl --install

# Reboot if prompted
```

After reboot:

```powershell
# Set WSL2 as default
wsl --set-default-version 2

# Install Ubuntu
wsl --install -d Ubuntu
```

#### 2. Build CRuntime

From Windows (PowerShell or CMD):

```powershell
# Navigate to cruntime directory
cd cruntime

# Build using PowerShell script
.\windows\build.ps1

# Or build and install
.\windows\build.ps1 -Install

# Or build and test
.\windows\build.ps1 -Install -Test
```

Or using the batch file:

```cmd
cd cruntime
windows\build.bat
```

#### 3. Run Containers

From PowerShell/CMD:

```powershell
# Run single command
wsl sudo cruntime run alpine echo "Hello from container"

# Interactive shell
wsl sudo cruntime run alpine /bin/sh

# With resource limits
wsl sudo cruntime run --memory 512M --cpus 1.0 alpine /bin/sh
```

Or enter WSL and use normally:

```bash
wsl
sudo cruntime run alpine /bin/sh
```

### WSL2 Integration Details

#### File Access

Your Windows files are accessible in WSL at `/mnt/c/`, `/mnt/d/`, etc:

```bash
# From Windows: C:\Users\YourName\project
# In WSL: /mnt/c/Users/YourName/project

# Mount Windows volume in container
wsl sudo cruntime run -v /mnt/c/data:/data alpine ls /data
```

#### Network Access

WSL2 containers can:
- Access Windows network
- Expose ports to Windows host
- Access Windows localhost

```bash
# Expose port to Windows
wsl sudo cruntime run -p 8080:80 alpine httpd

# Access from Windows browser: http://localhost:8080
```

#### Performance

WSL2 uses a real Linux kernel, so performance is near-native:
- Full namespace support
- Full cgroups v2 support
- Native filesystem performance
- No virtualization overhead for Linux syscalls

### Troubleshooting WSL2

#### WSL2 Not Available

```powershell
# Check Windows version
winver  # Need Windows 10 2004+ or Windows 11

# Enable required features
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart

# Reboot
shutdown /r /t 0
```

#### Check WSL Version

```powershell
wsl --list --verbose

# If VERSION is 1, upgrade to 2
wsl --set-version Ubuntu 2
```

#### Kernel Update

```powershell
# Download and install WSL2 kernel update
# https://aka.ms/wsl2kernel
```

#### Memory Issues

Create/edit `%USERPROFILE%\.wslconfig`:

```ini
[wsl2]
memory=4GB
processors=2
swap=2GB
```

Restart WSL:

```powershell
wsl --shutdown
```

---

## Method 2: Cross-Compilation (MinGW)

**Build Linux binaries from Windows for deployment on Linux servers.**

### Prerequisites

- MinGW-w64 or MSYS2
- Make for Windows

### Setup MSYS2

1. Download MSYS2 from https://www.msys2.org/
2. Install and open MSYS2 UCRT64 terminal
3. Install toolchain:

```bash
pacman -S base-devel mingw-w64-ucrt-x86_64-gcc make
```

### Build for Linux

Create `Makefile.mingw`:

```makefile
# Cross-compilation for Linux target from Windows
CC = x86_64-linux-gnu-gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -D_GNU_SOURCE
LDFLAGS = 
TARGET = cruntime

# Include main Makefile structure
include Makefile

# Override for cross-compilation
.PHONY: cross
cross:
	@echo "Building Linux binary on Windows..."
	$(MAKE) -f Makefile.mingw all CC=x86_64-linux-gnu-gcc
```

Build:

```bash
make -f Makefile.mingw cross
```

**Note**: The binary runs on Linux, not Windows. Use WSL2 to test or deploy to Linux server.

---

## Method 3: Windows Native Containers

**For true Windows container support, you'd need a completely different implementation.**

### Windows Container Architecture

Windows uses different isolation mechanisms:

```
Windows Containers
├── Process Isolation (Windows Server Containers)
│   ├── Job Objects (resource limits)
│   ├── Silos (namespace-like isolation)
│   └── Virtual Accounts (security)
└── Hyper-V Isolation (stronger isolation)
    └── Lightweight Hyper-V VM per container
```

### Windows Implementation Sketch

A Windows-native version would require:

```c
// Windows equivalent structures
#include <windows.h>
#include <winternl.h>
#include <jobapi2.h>

// Instead of Linux namespaces
typedef struct {
    HANDLE job_object;          // Instead of cgroups
    HANDLE silo_object;         // Instead of namespaces
    LPVOID virtual_account;     // Instead of user namespaces
    NETWORK_NAMESPACE net_ns;   // HNS for networking
} windows_container_t;

// Instead of clone()
HANDLE CreateProcessInSilo(...);

// Instead of cgroups
SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, ...);

// Instead of veth pairs
HNS (Host Network Service) APIs
```

### Using Windows Containers (Docker Desktop)

For production Windows containers, use Docker Desktop:

```powershell
# Install Docker Desktop for Windows
winget install Docker.DockerDesktop

# Run Windows containers
docker run --isolation=process mcr.microsoft.com/windows/nanoserver:ltsc2022 cmd

# Or Hyper-V isolation
docker run --isolation=hyperv mcr.microsoft.com/windows/servercore:ltsc2022 cmd
```

---

## Comparison

| Method | Pros | Cons | Use Case |
|--------|------|------|----------|
| **WSL2** | Full Linux functionality, native performance, easy | Requires WSL2 | Development, testing |
| **Cross-compile** | Build on Windows, deploy anywhere | Binary runs on Linux only | CI/CD, build servers |
| **Native Windows** | True Windows containers | Completely different implementation | Production Windows apps |

---

## Recommended: WSL2 Development Workflow

### Setup

```powershell
# One-time setup
wsl --install -d Ubuntu
cd cruntime
.\windows\build.ps1 -Install
```

### Daily Development

```powershell
# Edit files in Windows (VS Code, Visual Studio, etc.)
code .

# Build in WSL
.\windows\build.ps1

# Test in WSL
wsl sudo cruntime run alpine /bin/sh
```

### IDE Integration

#### Visual Studio Code

Install extensions:
- Remote - WSL
- C/C++

Open folder in WSL:

```powershell
code --remote wsl+Ubuntu /path/to/cruntime
```

Build and debug directly in WSL environment.

#### Visual Studio 2022

Use WSL2 toolset:

1. Install "Linux development with C++" workload
2. Tools → Options → Cross Platform → Connection Manager
3. Add WSL connection
4. Build and debug directly

---

## Windows-Specific Features

### Volume Mounting

Mount Windows directories:

```powershell
# Mount C:\data into container
wsl sudo cruntime run -v /mnt/c/data:/data alpine ls /data
```

### Port Forwarding

Ports are automatically forwarded to Windows:

```powershell
wsl sudo cruntime run -p 8080:80 alpine httpd

# Access from Windows: http://localhost:8080
# Access from network: http://<windows-ip>:8080
```

### Environment Variables

Pass Windows environment variables:

```powershell
wsl sudo cruntime run -e "USER=$env:USERNAME" alpine env
```

---

## Performance Tips

### WSL2 Filesystem

- Keep project files in WSL filesystem (`/home/user/`) for best performance
- Access Windows files via `/mnt/c/` when needed
- Use `wsl$` network share from Windows: `\\wsl$\Ubuntu\home\user\cruntime`

### Resource Limits

Configure WSL2 resources in `%USERPROFILE%\.wslconfig`:

```ini
[wsl2]
memory=8GB
processors=4
swap=4GB
localhostForwarding=true
```

### Docker Desktop Integration

If you have Docker Desktop, it uses WSL2 backend:

```powershell
# CRuntime and Docker can coexist
wsl sudo cruntime run alpine /bin/sh
docker run alpine /bin/sh
```

---

## Build Scripts Reference

### PowerShell Script

```powershell
# Build only
.\windows\build.ps1

# Build and install
.\windows\build.ps1 -Install

# Build, install, and test
.\windows\build.ps1 -Install -Test

# Clean build artifacts
.\windows\build.ps1 -Clean

# Show help
.\windows\build.ps1 -Help
```

### Batch File

```cmd
REM Simple wrapper
windows\build.bat

REM With arguments (passed to PowerShell script)
windows\build.bat -Install
```

---

## Complete Windows Setup Example

```powershell
# 1. Enable WSL2 (if not already)
wsl --install

# Reboot if needed, then:

# 2. Install Ubuntu
wsl --install -d Ubuntu

# 3. Extract and navigate to CRuntime
cd C:\Users\YourName\Projects
tar -xzf cruntime.tar.gz
cd cruntime

# 4. Build and install
.\windows\build.ps1 -Install -Test

# 5. Run your first container
wsl sudo cruntime run alpine echo "Hello from Windows!"

# 6. Interactive shell
wsl sudo cruntime run alpine /bin/sh

# 7. Web server example
wsl sudo cruntime run -p 8080:80 alpine httpd
# Open browser: http://localhost:8080
```

---

## Next Steps

1. **Read QUICKSTART.md** for basic usage
2. **Read README.md** for complete documentation
3. **Check examples/** for sample configurations
4. **Read ARCHITECTURE.md** for technical details

## Support

For Windows-specific issues:
- WSL2 issues: https://docs.microsoft.com/windows/wsl/
- Docker Desktop: https://docs.docker.com/desktop/windows/

For CRuntime issues:
- Check logs with `--debug` flag
- Review ARCHITECTURE.md for internals
