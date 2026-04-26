# CRuntime Technical Architecture

## Table of Contents
1. [Overview](#overview)
2. [Core Components](#core-components)
3. [Namespace Isolation](#namespace-isolation)
4. [Cgroup Resource Management](#cgroup-resource-management)
5. [Network Architecture](#network-architecture)
6. [Storage Layer](#storage-layer)
7. [Container Lifecycle](#container-lifecycle)
8. [Security Model](#security-model)

## Overview

CRuntime is a production-grade container runtime that demonstrates the fundamental concepts behind containerization technology. It implements the core features found in Docker and other container platforms, built entirely in C for maximum control and minimal overhead.

### Design Principles

- **Minimalism**: Only essential features, no bloat
- **Performance**: C implementation for speed and low resource usage
- **Standards Compliance**: Follows Linux kernel conventions
- **Security**: Defense in depth with multiple isolation layers
- **Production Ready**: Proper error handling and resource cleanup

## Core Components

### Runtime Context

The runtime context (`runtime_ctx_t`) maintains global state:

```c
typedef struct {
    char runtime_dir[MAX_PATH_LEN];    // /var/run/cruntime
    char state_dir[MAX_PATH_LEN];      // /var/lib/cruntime/state
    char image_dir[MAX_PATH_LEN];      // /var/lib/cruntime/images
    char network_dir[MAX_PATH_LEN];    // /var/run/cruntime/netns
    bool use_systemd;
} runtime_ctx_t;
```

### Container Representation

Each container is represented by a `container_t` structure containing:

- Configuration (command, env vars, limits)
- Runtime state (PID, status)
- Resource paths (cgroup, network namespace)
- Lifecycle timestamps

## Namespace Isolation

### Overview

Linux namespaces provide the foundation for container isolation. CRuntime uses all available namespace types:

```c
typedef struct {
    bool pid;      // Process ID isolation
    bool net;      // Network stack isolation
    bool mnt;      // Filesystem mount isolation
    bool uts;      // Hostname isolation
    bool ipc;      // Inter-process communication isolation
    bool user;     // User/group ID isolation
    bool cgroup;   // Cgroup view isolation
} namespace_config_t;
```

### Implementation Flow

1. **Clone with Namespaces**
```c
int clone_flags = 0;
if (config->namespaces.pid) flags |= CLONE_NEWPID;
if (config->namespaces.net) flags |= CLONE_NEWNET;
// ... etc

pid_t pid = clone(container_init, stack_top, clone_flags, &config);
```

2. **Setup Inside Container**
- Mount proc, sys, dev filesystems
- Configure hostname (UTS namespace)
- Setup user mappings (user namespace)
- Pivot root to container rootfs

### PID Namespace

The PID namespace creates a new process tree:

```
Host PID Namespace          Container PID Namespace
─────────────────────       ───────────────────────
PID 1 (init)               
├── PID 1234 (cruntime)     
│   └── PID 1235 ────────→  PID 1 (container init)
│                           ├── PID 2 (app)
│                           └── PID 3 (helper)
```

Benefits:
- Container processes start from PID 1
- Isolated from host process tree
- Clean process hierarchy
- Proper signal handling

### Mount Namespace

Creates isolated filesystem view:

```c
// Make root private to prevent mount propagation
mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL);

// Bind mount container rootfs
mount(config->rootfs, config->rootfs, NULL, MS_BIND | MS_REC, NULL);

// Mount essential filesystems
mount("proc", "/proc", "proc", 0, NULL);
mount("sysfs", "/sys", "sysfs", 0, NULL);
mount("devtmpfs", "/dev", "devtmpfs", MS_NOSUID, NULL);

// Pivot root
pivot_root(new_root, old_root);
chdir("/");
umount2(old_root, MNT_DETACH);
```

### Network Namespace

Provides dedicated network stack:
- Separate routing tables
- Firewall rules
- Network interfaces

Connected via veth pairs to host bridge.

### User Namespace

Maps container root to unprivileged user on host:

```c
// /proc/<pid>/uid_map
0 1000 1  // Container UID 0 → Host UID 1000

// /proc/<pid>/gid_map
0 1000 1  // Container GID 0 → Host GID 1000
```

Enables rootless containers with reduced privilege.

## Cgroup Resource Management

### Cgroups v2 Unified Hierarchy

CRuntime uses cgroups v2 for resource control:

```
/sys/fs/cgroup/
└── cruntime/
    └── <container-id>/
        ├── cgroup.procs        # Process list
        ├── cgroup.controllers  # Available controllers
        ├── memory.max          # Memory limit
        ├── memory.current      # Current usage
        ├── cpu.max             # CPU quota/period
        ├── cpu.weight          # CPU shares
        ├── pids.max            # Process limit
        └── io.weight           # I/O priority
```

### Resource Limit Application

```c
int cgroup_apply_limits(const char *cgroup_path, resource_limits_t *limits) {
    // Memory limit
    if (limits->memory_limit > 0) {
        write_cgroup_file("memory.max", limit_value);
    }
    
    // CPU quota (e.g., 1.5 cores = 150000/100000)
    if (limits->cpu_quota > 0) {
        write_cgroup_file("cpu.max", "quota period");
    }
    
    // PIDs limit
    if (limits->pids_limit > 0) {
        write_cgroup_file("pids.max", limit_value);
    }
}
```

### Pause/Resume Implementation

Uses cgroup freezer:

```c
// Pause
write_cgroup_file("cgroup.freeze", "1");

// Resume
write_cgroup_file("cgroup.freeze", "0");
```

Freezes all processes atomically without SIGSTOP.

## Network Architecture

### Bridge Network Setup

```
┌──────────────────────────────────┐
│         Host Network             │
│                                  │
│  ┌────────────────────────────┐ │
│  │  cr0 Bridge (172.17.0.1)   │ │
│  │                            │ │
│  │  ┌──────┐  ┌──────┐       │ │
│  │  │veth0 │  │veth1 │       │ │
│  │  └───┬──┘  └───┬──┘       │ │
│  └──────│─────────│──────────┘ │
│         │         │              │
└─────────│─────────│──────────────┘
          │         │
     ┌────│────┐    │
     │    │    │    │
     │ ┌──▼──┐ │ ┌──▼──┐
     │ │eth0 │ │ │eth0 │
     │ └─────┘ │ └─────┘
     │Container│ │Container
     │    A   │ │    B
     └────────┘ └────────┘
```

### Implementation Steps

1. **Create Bridge**
```bash
ip link add name cr0 type bridge
ip addr add 172.17.0.1/16 dev cr0
ip link set cr0 up
```

2. **Create veth Pair**
```bash
ip link add veth<container-id> type veth peer name eth0
ip link set veth<container-id> master cr0
ip link set eth0 netns <container-pid>
```

3. **Configure Container Interface**
```bash
nsenter -t <pid> -n ip addr add 172.17.0.10/16 dev eth0
nsenter -t <pid> -n ip link set eth0 up
nsenter -t <pid> -n ip route add default via 172.17.0.1
```

4. **Enable NAT**
```bash
iptables -t nat -A POSTROUTING -s 172.17.0.0/16 ! -o cr0 -j MASQUERADE
```

### Port Forwarding

DNAT rules for host-to-container port mapping:

```bash
# Forward host:8080 to container:80
iptables -t nat -A PREROUTING -p tcp --dport 8080 \
  -j DNAT --to-destination 172.17.0.10:80

# Allow forwarding
iptables -A FORWARD -p tcp -d 172.17.0.10 --dport 80 -j ACCEPT
```

## Storage Layer

### Overlay Filesystem

Uses OverlayFS for efficient layered storage:

```
Merged View (Container sees this)
    ↑
    ├── upperdir (RW) - Container's changes
    │   /var/lib/cruntime/overlay/<id>/upper
    │
    └── lowerdir (RO) - Image layers
        ├── layer-n (newest)
        ├── layer-2
        ├── layer-1
        └── layer-0 (base)
```

### Mount Command

```c
mount("overlay", merged_path, "overlay", 0,
      "lowerdir=layer0:layer1:layer2,"
      "upperdir=/path/to/upper,"
      "workdir=/path/to/work");
```

### Layer Structure

Each layer is a directory containing filesystem changes:

```
/var/lib/cruntime/images/
└── layers/
    ├── sha256-abc123.../
    │   ├── bin/
    │   ├── lib/
    │   └── etc/
    ├── sha256-def456.../
    │   └── app/
    └── sha256-ghi789.../
        └── config/
```

### Copy-on-Write

When container modifies a file:
1. File is copied from lowerdir to upperdir
2. Modification happens in upperdir
3. Original in lowerdir remains unchanged
4. Merged view shows modified version

## Container Lifecycle

### State Machine

```
    ┌─────────┐
    │ CREATED │
    └────┬────┘
         │ start()
         ↓
    ┌─────────┐     pause()    ┌────────┐
    │ RUNNING │ ────────────→  │ PAUSED │
    └────┬────┘  ←──────────── └────────┘
         │           resume()
         │ stop()
         ↓
    ┌─────────┐
    │ STOPPED │
    └────┬────┘
         │ delete()
         ↓
    ┌─────────┐
    │ DELETED │
    └─────────┘
```

### Create Process

```c
int container_create(runtime_ctx_t *ctx, container_config_t *config,
                    container_t **out_container) {
    // 1. Allocate container structure
    container_t *container = malloc(sizeof(container_t));
    
    // 2. Create cgroup
    cgroup_create(config->id, container->cgroup_path);
    
    // 3. Apply resource limits
    cgroup_apply_limits(container->cgroup_path, &config->limits);
    
    // 4. Persist state
    save_container_state(ctx, container);
    
    container->state = CONTAINER_CREATED;
    *out_container = container;
}
```

### Start Process

```c
int container_start(runtime_ctx_t *ctx, container_t *container) {
    // 1. Get namespace flags
    int flags = namespace_get_clone_flags(&container->config.namespaces);
    
    // 2. Clone container init process
    pid_t pid = clone(container_init, stack, flags, &container->config);
    
    // 3. Setup user namespace mappings (if enabled)
    if (container->config.namespaces.user) {
        write_uid_gid_maps(pid, container->config.uid, container->config.gid);
    }
    
    // 4. Add to cgroup
    cgroup_add_process(container->cgroup_path, pid);
    
    // 5. Setup networking
    if (container->config.namespaces.net) {
        network_setup_container(container);
    }
    
    container->state = CONTAINER_RUNNING;
    container->init_pid = pid;
}
```

### Container Init Function

Runs inside container namespace:

```c
static int container_init(void *arg) {
    container_config_t *config = arg;
    
    // 1. Setup hostname
    sethostname(config->hostname);
    
    // 2. Setup mounts
    namespace_setup_mounts(config);
    
    // 3. Pivot root
    pivot_root(config->rootfs);
    
    // 4. Change directory
    chdir(config->working_dir);
    
    // 5. Setup environment
    for (int i = 0; i < config->num_env; i++) {
        putenv(config->env[i]);
    }
    
    // 6. Drop capabilities (if not privileged)
    if (!config->privileged) {
        drop_capabilities();
    }
    
    // 7. Execute command
    execvp(config->command, config->args);
}
```

## Security Model

### Multi-Layer Defense

1. **Namespace Isolation**: Process, network, filesystem separation
2. **Resource Limits**: Cgroup-based DoS prevention
3. **Capability Dropping**: Reduced privilege inside container
4. **Read-Only Root**: Optional immutable filesystem
5. **User Namespaces**: UID/GID mapping for privilege reduction

### Capability Management

Non-privileged containers drop dangerous capabilities:

```c
// Drop all capabilities
for (int i = 0; i <= CAP_LAST_CAP; i++) {
    prctl(PR_CAPBSET_DROP, i, 0, 0, 0);
}
```

Keeps only essential capabilities like:
- CAP_CHOWN
- CAP_DAC_OVERRIDE (limited)
- CAP_FOWNER
- CAP_SETGID
- CAP_SETUID

### Attack Surface Reduction

- No setuid binaries in container
- No device access (unless explicitly mounted)
- Limited syscalls via seccomp (future enhancement)
- AppArmor/SELinux profiles (future enhancement)

## Performance Considerations

### Clone vs Fork

Using `clone()` instead of `fork()` + `unshare()`:
- Single syscall for namespace creation
- Faster container startup
- Atomic namespace setup

### Copy-on-Write

OverlayFS provides efficient storage:
- Shared read-only base layers
- Only differences stored per container
- Minimal disk usage
- Fast container creation

### Cgroup Overhead

Minimal performance impact:
- Accounting happens in kernel
- No userspace polling
- Event-driven notifications

## Comparison with Other Runtimes

| Feature | CRuntime | runc | crun |
|---------|----------|------|------|
| Language | C | Go | C |
| Size | ~100KB | ~10MB | ~200KB |
| OCI Support | Planned | Yes | Yes |
| Startup Time | Very Fast | Fast | Very Fast |
| Memory Usage | Minimal | Moderate | Minimal |

## Future Enhancements

1. **OCI Compliance**: Full OCI runtime spec support
2. **Seccomp Filters**: Syscall filtering
3. **AppArmor/SELinux**: MAC integration
4. **CRIU Integration**: Checkpoint/restore
5. **Image Builder**: Dockerfile support
6. **Registry Client**: OCI distribution spec
7. **Rootless Mode**: Fully unprivileged containers
8. **Multi-arch**: ARM, RISC-V support
