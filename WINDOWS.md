# CRuntime - Windows Edition

**Full Linux container support on Windows via WSL2**

## Quick Start (2 Minutes)

### Prerequisites
- Windows 10 (2004+) or Windows 11
- Administrator access

### Setup

```powershell
# 1. Install WSL2 (PowerShell as Admin)
wsl --install

# 2. Reboot, then navigate to project
cd path\to\cruntime

# 3. Build and install
.\windows\build.ps1 -Install -Test

# 4. Run container
wsl sudo cruntime run alpine echo "Hello!"
```

## What You Get

✅ **Full Linux container functionality on Windows**
- All namespaces (PID, mount, network, UTS, IPC, user, cgroup)
- Complete cgroups v2 resource management
- Full networking with port forwarding to Windows
- Volume mounting (Windows ↔ Container)
- Near-native performance

✅ **Windows Integration**
- Access containers from Windows: `http://localhost:8080`
- Mount Windows directories: `-v /mnt/c/data:/data`
- Edit code in Windows, build in WSL
- Full VS Code and Visual Studio support

✅ **Build Options**
- PowerShell scripts (Windows-native)
- Batch files (CMD compatibility)
- CMake (cross-platform)
- Traditional Make (WSL)

## Documentation

| Document | Description |
|----------|-------------|
| **[windows/README.md](windows/README.md)** | Quick start guide |
| **[windows/WINDOWS_GUIDE.md](windows/WINDOWS_GUIDE.md)** | Complete Windows integration guide |
| **[README.md](README.md)** | Full CRuntime documentation |
| **[QUICKSTART.md](QUICKSTART.md)** | Basic usage examples |

## Build Methods

### Method 1: PowerShell (Recommended)

```powershell
# Standard build
.\windows\build.ps1

# Build and install
.\windows\build.ps1 -Install

# Clean build
.\windows\build.ps1 -Clean

# Run tests
.\windows\build.ps1 -Test
```

### Method 2: Batch File

```cmd
windows\build.bat
```

### Method 3: CMake

```powershell
.\windows\build-cmake.ps1 -Install
```

### Method 4: Make (in WSL)

```bash
wsl
cd ~/cruntime
make
sudo make install
```

## IDE Support

### Visual Studio Code

```powershell
# Install extensions
code --install-extension ms-vscode-remote.remote-wsl
code --install-extension ms-vscode.cpptools

# Open in WSL mode
code --remote wsl+Ubuntu .
```

Features:
- IntelliSense (code completion)
- Debugging with breakpoints (F5)
- Integrated build (Ctrl+Shift+B)
- Git integration
- Terminal access

### Visual Studio 2022

1. Open `CMakeLists.txt`
2. Select "WSL-GCC-Debug" configuration
3. Build and debug

### CLion

Import as CMake project, configure WSL toolchain.

## Common Tasks

### Run Container

```powershell
# Interactive shell
wsl sudo cruntime run alpine /bin/sh

# Specific command
wsl sudo cruntime run alpine echo "Hello"

# Detached
wsl sudo cruntime run --detach alpine sleep 3600
```

### Resource Limits

```powershell
# Memory limit
wsl sudo cruntime run --memory 512M alpine /bin/sh

# CPU limit
wsl sudo cruntime run --cpus 1.0 alpine /bin/sh

# Combined
wsl sudo cruntime run --memory 1G --cpus 2.0 alpine /bin/sh
```

### Networking

```powershell
# Port forwarding (accessible from Windows!)
wsl sudo cruntime run -p 8080:80 alpine httpd

# Open browser: http://localhost:8080
# Or from network: http://<your-pc-ip>:8080
```

### Volumes

```powershell
# Mount Windows directory
wsl sudo cruntime run -v /mnt/c/Users/YourName/data:/data alpine ls /data

# Mount current directory
wsl sudo cruntime run -v "$(wsl wslpath -a $PWD):/workspace" alpine ls /workspace
```

## File Access

### Windows → WSL

From Windows Explorer:
```
\\wsl$\Ubuntu\home\username\cruntime
```

### WSL → Windows

From WSL terminal:
```bash
cd /mnt/c/Users/YourName/
```

## Development Workflow

### Recommended Setup

1. **Edit in Windows** - Use VS Code, Visual Studio, or any editor
2. **Build in WSL** - Run `.\windows\build.ps1`
3. **Test in WSL** - Run containers with `wsl sudo cruntime ...`
4. **Debug in IDE** - Use VS Code Remote-WSL or Visual Studio

### Example: Web Development

```powershell
# Terminal 1: Build
.\windows\build.ps1 -Install

# Terminal 2: Run web server
wsl sudo cruntime run -p 8080:80 -v /mnt/c/www:/var/www alpine httpd

# Browser: http://localhost:8080
# Edit files in C:\www, reload browser to see changes
```

## Troubleshooting

### WSL2 Issues

```powershell
# Update WSL
wsl --update

# Check status
wsl --status

# List distributions
wsl --list --verbose

# Set default version
wsl --set-default-version 2

# Restart WSL
wsl --shutdown
```

### Build Issues

```powershell
# Clean and rebuild
.\windows\build.ps1 -Clean
.\windows\build.ps1

# Check dependencies
wsl bash -c "cd $(wsl wslpath -a $PWD) && make check"

# Manual dependency install
wsl sudo apt-get update
wsl sudo apt-get install -y build-essential iproute2 iptables util-linux
```

### Network Issues

```powershell
# Check WSL networking
wsl ip addr
wsl ping google.com

# Check Windows firewall
# Allow WSL through Windows Defender Firewall

# Reset WSL networking
wsl --shutdown
wsl
```

### Performance

For best performance, keep files in WSL filesystem:

```bash
# In WSL
cd ~
cp -r /mnt/c/Users/YourName/cruntime .
cd cruntime
make
```

## Advanced Features

### Container Management

```powershell
# List containers
wsl sudo cruntime ps

# Stop container
wsl sudo cruntime stop <container-id>

# Remove container
wsl sudo cruntime rm <container-id>

# View logs
wsl sudo cruntime logs <container-id>

# Container stats
wsl sudo cruntime stats <container-id>
```

### Multi-Container

```powershell
# Run database
wsl sudo cruntime run --name db -p 5432:5432 --detach alpine postgres

# Run web app
wsl sudo cruntime run --name web -p 8080:80 --detach alpine httpd

# Both accessible from Windows!
```

## Testing

```powershell
# Quick test
.\windows\test.ps1

# Full test suite
.\windows\test.ps1 -Full
```

## Performance Comparison

| Metric | WSL2 | Native Linux | Docker Desktop |
|--------|------|--------------|----------------|
| Startup | ~1s | ~1s | ~2s |
| I/O (WSL FS) | 100% | 100% | 100% |
| I/O (Windows FS) | ~80% | N/A | ~80% |
| Network | 100% | 100% | 100% |
| Memory | Shared | N/A | Separate VM |

## FAQ

**Q: Can I run this without WSL2?**
A: No, the runtime requires Linux kernel features (namespaces, cgroups).

**Q: Is this production-ready?**
A: Yes for development/testing. For production Windows containers, use Docker Desktop or native Windows containers.

**Q: Can containers access Windows apps?**
A: Yes via localhost. Windows and WSL2 share the network stack.

**Q: Can I use both Docker and CRuntime?**
A: Yes, they can coexist. Docker Desktop uses WSL2 backend too.

**Q: What about performance?**
A: WSL2 uses a real Linux kernel, so performance is near-native.

**Q: Can I build Windows containers?**
A: No, this builds Linux containers. For Windows containers, see windows/WINDOWS_GUIDE.md.

## Resources

- **WSL2 Docs**: https://docs.microsoft.com/windows/wsl/
- **VS Code Remote**: https://code.visualstudio.com/docs/remote/wsl
- **Visual Studio WSL**: https://docs.microsoft.com/visualstudio/linux/

## Support

For issues:
1. Check [windows/WINDOWS_GUIDE.md](windows/WINDOWS_GUIDE.md)
2. Review WSL logs: `wsl dmesg`
3. Check Windows Event Viewer
4. Run diagnostics: `.\windows\test.ps1`

---

**Ready to start? Run:**

```powershell
.\windows\build.ps1 -Install -Test
```
