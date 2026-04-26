# CRuntime - Production Container Runtime

A production-grade container runtime written in C from scratch, implementing full namespace isolation, cgroup resource limits, overlay filesystems, and container networking.

## Features

### Core Isolation
- **Linux Namespaces**: Full namespace isolation (PID, mount, network, UTS, IPC, user, cgroup)
- **Process Isolation**: Complete process tree isolation with PID namespace
- **Filesystem Isolation**: Overlay filesystem with layered image support
- **Network Isolation**: Dedicated network namespace per container
- **Hostname Isolation**: Separate hostname and domain configuration

### Resource Management
- **Memory Limits**: Configurable memory and swap limits via cgroups v2
- **CPU Control**: CPU shares and quota management
- **PIDs Limit**: Maximum process count enforcement
- **Block I/O**: I/O weight and throttling

### Networking
- **Bridge Networks**: Software-defined bridge networking
- **NAT/Forwarding**: Automatic NAT configuration with iptables
- **Port Mapping**: Host-to-container port forwarding
- **IP Address Management**: Automatic or manual IP allocation
- **veth Pairs**: Virtual ethernet device pairs for container connectivity

### Storage
- **Overlay Filesystem**: Efficient layered storage with OverlayFS
- **Image Layers**: Support for multi-layer container images
- **Copy-on-Write**: Efficient storage with shared base layers
- **Volume Mounts**: Bind mount support for persistent storage

### Container Lifecycle
- Create, start, stop, pause, resume, kill operations
- Graceful shutdown with configurable timeouts
- Container state persistence
- Automatic resource cleanup

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    CLI Interface                        │
└─────────────────────────────────────────────────────────┘
                          │
┌─────────────────────────┼─────────────────────────────┐
│                   Core Runtime                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐             │
│  │Container │  │Container │  │Container │             │
│  │Lifecycle │  │State Mgr │  │  Exec    │             │
│  └──────────┘  └──────────┘  └──────────┘             │
└─────────────────────────┬─────────────────────────────┘
                          │
        ┌─────────────────┼─────────────────┐
        │                 │                 │
┌───────▼────────┐ ┌──────▼──────┐ ┌───────▼────────┐
│   Namespace    │ │   Cgroup    │ │    Network     │
│   Management   │ │  Management │ │   Management   │
│                │ │             │ │                │
│ • PID          │ │ • Memory    │ │ • Bridge       │
│ • Mount        │ │ • CPU       │ │ • veth Pairs   │
│ • Network      │ │ • PIDs      │ │ • Port Forward │
│ • UTS          │ │ • Block I/O │ │ • NAT/iptables │
│ • IPC          │ │             │ │                │
│ • User         │ │             │ │                │
└────────────────┘ └─────────────┘ └────────────────┘
        │
┌───────▼────────┐
│    Storage     │
│   Management   │
│                │
│ • OverlayFS    │
│ • Image Layers │
│ • Volumes      │
│                │
└────────────────┘
```

## Requirements

### System Requirements
- Linux kernel 5.0+ with cgroups v2 enabled
- Root privileges (required for namespaces and cgroups)
- x86_64 or ARM64 architecture

### Required Kernel Features
```bash
# Check namespace support
ls /proc/self/ns/

# Check cgroups v2
mount | grep cgroup2
ls /sys/fs/cgroup/cgroup.controllers

# Check overlay filesystem
modprobe overlay
cat /proc/filesystems | grep overlay
```

### Required Tools
- `iproute2` - for network management (ip command)
- `iptables` - for NAT and port forwarding
- `util-linux` - for nsenter command
- GCC or Clang compiler

Install dependencies on Ubuntu/Debian:
```bash
sudo apt-get install build-essential iproute2 iptables util-linux
```

## Building

### Quick Build
```bash
make
```

### Build with Debug Symbols
```bash
make debug
```

### Build Static Binary
```bash
make static
```

### Install System-Wide
```bash
sudo make install
```

### Check Dependencies
```bash
make check
```

## Usage

### Basic Container

Run a simple container:
```bash
sudo cruntime run ubuntu /bin/bash
```

Run with custom name:
```bash
sudo cruntime run --name mycontainer ubuntu /bin/sh
```

### Resource Limits

Limit memory to 512MB:
```bash
sudo cruntime run --memory 512M ubuntu stress --vm 1
```

Limit CPU to 50% of one core:
```bash
sudo cruntime run --cpus 0.5 ubuntu stress --cpu 1
```

Combined limits:
```bash
sudo cruntime run --memory 1G --cpus 2.0 ubuntu /app/server
```

### Networking

Run with custom network:
```bash
sudo cruntime run --network mynet --ip 172.17.0.10/16 ubuntu
```

Port forwarding:
```bash
sudo cruntime run -p 8080:80 -p 8443:443/tcp ubuntu nginx
```

### Volumes

Mount host directory:
```bash
sudo cruntime run -v /host/data:/container/data ubuntu
```

Multiple volume mounts:
```bash
sudo cruntime run \
  -v /host/data:/data \
  -v /host/config:/etc/app \
  ubuntu /app/server
```

### Environment Variables

Set environment variables:
```bash
sudo cruntime run \
  -e DATABASE_URL=postgres://localhost/db \
  -e API_KEY=secret \
  ubuntu /app/server
```

### Working Directory

Set working directory:
```bash
sudo cruntime run --workdir /app ubuntu node server.js
```

### Complete Example

```bash
sudo cruntime run \
  --name web-server \
  --hostname web01 \
  --memory 2G \
  --cpus 1.5 \
  --network production \
  --ip 172.17.0.20/16 \
  -p 80:8080/tcp \
  -p 443:8443/tcp \
  -v /data/web:/var/www \
  -v /data/config:/etc/nginx \
  -e ENVIRONMENT=production \
  -e LOG_LEVEL=info \
  --workdir /var/www \
  --detach \
  ubuntu \
  nginx -g "daemon off;"
```

## Advanced Features

### Container Management

List running containers:
```bash
sudo cruntime ps
```

Stop a container gracefully:
```bash
sudo cruntime stop <container-id>
```

Kill a container:
```bash
sudo cruntime kill <container-id>
```

Remove a container:
```bash
sudo cruntime rm <container-id>
```

Pause/resume:
```bash
sudo cruntime pause <container-id>
sudo cruntime unpause <container-id>
```

### Container Stats

View resource usage:
```bash
sudo cruntime stats <container-id>
```

### Execute in Running Container

```bash
sudo cruntime exec <container-id> /bin/bash
```

### View Logs

```bash
sudo cruntime logs <container-id>
sudo cruntime logs --follow <container-id>
```

## Technical Details

### Namespace Implementation

CRuntime uses Linux namespaces for process isolation:

- **PID Namespace**: Container processes have their own PID namespace starting from PID 1
- **Mount Namespace**: Isolated filesystem view with pivot_root
- **Network Namespace**: Dedicated network stack with virtual interfaces
- **UTS Namespace**: Separate hostname and domain name
- **IPC Namespace**: Isolated inter-process communication
- **User Namespace**: UID/GID mapping for unprivileged containers
- **Cgroup Namespace**: Isolated cgroup hierarchy view

### Cgroup Resource Control

Using cgroups v2 unified hierarchy:

```
/sys/fs/cgroup/
└── cruntime/
    └── <container-id>/
        ├── cgroup.procs
        ├── memory.max
        ├── memory.current
        ├── cpu.max
        ├── cpu.weight
        ├── pids.max
        └── io.weight
```

### Network Architecture

```
Host Network
    │
    ├── cr0 (bridge) 172.17.0.1/16
    │   │
    │   ├── vethXXXX ──┐
    │   ├── vethYYYY ──┤
    │   └── vethZZZZ ──┤
    │                  │
Container Namespaces   │
    │                  │
    ├── Container A    │
    │   └── eth0 ──────┘  172.17.0.10/16
    │
    ├── Container B
    │   └── eth0 ─────────  172.17.0.11/16
    │
    └── Container C
        └── eth0 ─────────  172.17.0.12/16
```

### Storage Layering

```
Container Filesystem (merged)
    │
    ├── upperdir (read-write layer - container changes)
    │   └── /var/lib/cruntime/overlay/<container-id>/upper
    │
    └── lowerdir (read-only layers - image layers)
        ├── layer-3 (application layer)
        ├── layer-2 (dependencies)
        ├── layer-1 (runtime)
        └── layer-0 (base OS)
```

## Security Considerations

1. **Root Required**: Container runtime requires root for namespace/cgroup operations
2. **Capability Dropping**: Non-privileged containers drop capabilities
3. **User Namespaces**: Can be enabled for additional isolation
4. **Read-only Root**: Option to make container filesystem read-only
5. **Resource Limits**: Prevent resource exhaustion via cgroups
6. **Network Isolation**: Containers isolated by default

## Troubleshooting

### Container Won't Start

Check kernel features:
```bash
# Check namespaces
ls /proc/self/ns/

# Check cgroups v2
mount | grep cgroup2

# Check overlay support
cat /proc/filesystems | grep overlay
```

### Network Issues

Check bridge and iptables:
```bash
ip link show cr0
iptables -t nat -L POSTROUTING
iptables -L FORWARD
```

Enable IP forwarding:
```bash
sudo sysctl -w net.ipv4.ip_forward=1
```

### Permission Denied

Ensure running as root:
```bash
sudo cruntime run ...
```

Check cgroup permissions:
```bash
ls -la /sys/fs/cgroup/cruntime/
```

### Resource Limit Errors

Verify cgroup controllers are enabled:
```bash
cat /sys/fs/cgroup/cgroup.controllers
cat /sys/fs/cgroup/cgroup.subtree_control
```

## Comparison with Docker

| Feature | CRuntime | Docker |
|---------|----------|--------|
| Language | C | Go |
| Namespaces | All supported | All supported |
| Cgroups | v2 | v1 and v2 |
| Networking | Bridge + veth | Multiple drivers |
| Storage | OverlayFS | Multiple drivers |
| Image Format | Custom | OCI-compliant |
| Registry | Not yet | Yes |
| Swarm | No | Yes |
| Size | ~100KB binary | ~100MB+ |

## Future Enhancements

- [ ] OCI-compliant image format support
- [ ] Image registry integration (pull/push)
- [ ] Container image building (Dockerfile)
- [ ] Multiple network drivers (macvlan, ipvlan)
- [ ] AppArmor/SELinux integration
- [ ] Checkpoint/restore (CRIU)
- [ ] Container orchestration
- [ ] Multi-architecture support
- [ ] Rootless containers

## License

This is educational/production software. Use at your own risk.

## Contributing

This is a learning project demonstrating container runtime internals.
For production use, consider Docker, containerd, or CRI-O.

## Author

Built as a production-grade implementation of container runtime concepts.
