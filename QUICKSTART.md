# CRuntime Quick Start Guide

## Prerequisites

- Linux kernel 5.0+ with cgroups v2
- Root access
- GCC compiler
- iproute2, iptables, util-linux packages

## 5-Minute Setup

### 1. Install Dependencies (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y build-essential iproute2 iptables util-linux wget
```

### 2. Build CRuntime

```bash
cd cruntime
make
```

### 3. Run Setup Script

```bash
sudo ./setup.sh
```

This script will:
- Check system requirements
- Build and install CRuntime
- Download a minimal Alpine Linux rootfs
- Run a test container

## First Container

### Simple Command

```bash
sudo cruntime run alpine ls /
```

### Interactive Shell

```bash
sudo cruntime run alpine /bin/sh
```

### With Resource Limits

```bash
sudo cruntime run --memory 512M --cpus 1.0 alpine /bin/sh
```

## Common Use Cases

### Web Server

```bash
# Run nginx with port forwarding
sudo cruntime run \
  --name webserver \
  -p 8080:80 \
  --memory 512M \
  alpine \
  /usr/sbin/nginx
```

### Database

```bash
# Run with persistent storage
sudo cruntime run \
  --name database \
  -v /host/data:/var/lib/mysql \
  --memory 2G \
  alpine \
  /usr/bin/mysqld
```

### Batch Job

```bash
# Run detached with auto-cleanup
sudo cruntime run \
  --detach \
  --rm \
  --memory 1G \
  alpine \
  /app/batch-job.sh
```

## Container Management

```bash
# List containers
sudo cruntime ps

# Stop a container
sudo cruntime stop <container-id>

# View logs
sudo cruntime logs <container-id>

# Execute command in running container
sudo cruntime exec <container-id> /bin/sh

# Remove container
sudo cruntime rm <container-id>
```

## Troubleshooting

### Check System Compatibility

```bash
# Verify cgroups v2
mount | grep cgroup2

# Check namespaces
ls /proc/self/ns/

# Test overlay filesystem
sudo modprobe overlay
cat /proc/filesystems | grep overlay
```

### Enable IP Forwarding

```bash
sudo sysctl -w net.ipv4.ip_forward=1
# Make permanent:
echo "net.ipv4.ip_forward=1" | sudo tee -a /etc/sysctl.conf
```

### Permission Issues

Always run as root:
```bash
sudo cruntime <command>
```

### Network Issues

Check bridge:
```bash
ip link show cr0
```

Check iptables:
```bash
sudo iptables -t nat -L -n
```

## Next Steps

- Read [README.md](../README.md) for complete documentation
- See [ARCHITECTURE.md](docs/ARCHITECTURE.md) for technical details
- Check [examples/](examples/) for sample configurations

## Getting Help

If you encounter issues:

1. Check kernel version: `uname -r` (need 5.0+)
2. Verify cgroups v2: `ls /sys/fs/cgroup/cgroup.controllers`
3. Check required tools: `make check`
4. Review logs with `--debug` flag

## Uninstalling

```bash
sudo make uninstall
sudo rm -rf /var/lib/cruntime
sudo rm -rf /var/run/cruntime
```
