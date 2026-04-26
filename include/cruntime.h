#ifndef CRUNTIME_H
#define CRUNTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

/* Version */
#define CRUNTIME_VERSION "1.0.0"

/* Limits */
#define MAX_CONTAINER_NAME 256
#define MAX_PATH_LEN 4096
#define MAX_ENV_VARS 256
#define MAX_MOUNTS 64
#define MAX_PORTS 128

/* Container states */
typedef enum {
    CONTAINER_CREATED,
    CONTAINER_RUNNING,
    CONTAINER_PAUSED,
    CONTAINER_STOPPED,
    CONTAINER_DELETED
} container_state_t;

/* Namespace flags */
typedef struct {
    bool pid;      /* PID namespace */
    bool net;      /* Network namespace */
    bool mnt;      /* Mount namespace */
    bool uts;      /* UTS namespace (hostname) */
    bool ipc;      /* IPC namespace */
    bool user;     /* User namespace */
    bool cgroup;   /* Cgroup namespace */
} namespace_config_t;

/* Resource limits (cgroups) */
typedef struct {
    int64_t memory_limit;      /* Memory limit in bytes (-1 = unlimited) */
    int64_t memory_swap;       /* Memory + swap limit */
    int64_t cpu_shares;        /* CPU shares (relative weight) */
    int64_t cpu_quota;         /* CPU quota in microseconds */
    int64_t cpu_period;        /* CPU period in microseconds */
    int64_t pids_limit;        /* Max number of PIDs */
    int64_t blkio_weight;      /* Block I/O weight (10-1000) */
} resource_limits_t;

/* Mount point */
typedef struct {
    char source[MAX_PATH_LEN];
    char target[MAX_PATH_LEN];
    char fstype[64];
    unsigned long flags;
    char options[256];
} mount_point_t;

/* Port mapping */
typedef struct {
    uint16_t host_port;
    uint16_t container_port;
    char protocol[8];  /* tcp/udp */
} port_mapping_t;

/* Network configuration */
typedef struct {
    char bridge_name[64];
    char ip_address[64];
    char gateway[64];
    char subnet[64];
    port_mapping_t ports[MAX_PORTS];
    int num_ports;
    bool enable_nat;
} network_config_t;

/* Image layer */
typedef struct {
    char id[65];           /* SHA256 digest */
    char diff_path[MAX_PATH_LEN];
    size_t size;
} image_layer_t;

/* Container image */
typedef struct {
    char id[65];
    char name[MAX_CONTAINER_NAME];
    char tag[64];
    image_layer_t *layers;
    int num_layers;
    char rootfs_path[MAX_PATH_LEN];
    char metadata_path[MAX_PATH_LEN];
} container_image_t;

/* Container configuration */
typedef struct {
    char id[65];
    char name[MAX_CONTAINER_NAME];
    char hostname[256];
    char *command;
    char **args;
    int num_args;
    char **env;
    int num_env;
    char rootfs[MAX_PATH_LEN];
    char working_dir[MAX_PATH_LEN];
    
    namespace_config_t namespaces;
    resource_limits_t limits;
    network_config_t network;
    
    mount_point_t mounts[MAX_MOUNTS];
    int num_mounts;
    
    bool privileged;
    bool readonly_rootfs;
    uid_t uid;
    gid_t gid;
} container_config_t;

/* Running container */
typedef struct {
    container_config_t config;
    pid_t init_pid;
    container_state_t state;
    char cgroup_path[MAX_PATH_LEN];
    char netns_path[MAX_PATH_LEN];
    char bundle_path[MAX_PATH_LEN];
    int64_t created_at;
    int64_t started_at;
} container_t;

/* Container runtime context */
typedef struct {
    char runtime_dir[MAX_PATH_LEN];
    char state_dir[MAX_PATH_LEN];
    char image_dir[MAX_PATH_LEN];
    char network_dir[MAX_PATH_LEN];
    bool use_systemd;
} runtime_ctx_t;

/* API Functions */

/* Runtime initialization */
int runtime_init(runtime_ctx_t *ctx);
void runtime_cleanup(runtime_ctx_t *ctx);

/* Container lifecycle */
int container_create(runtime_ctx_t *ctx, container_config_t *config, container_t **container);
int container_start(runtime_ctx_t *ctx, container_t *container);
int container_stop(runtime_ctx_t *ctx, container_t *container, int timeout);
int container_kill(runtime_ctx_t *ctx, container_t *container, int signal);
int container_delete(runtime_ctx_t *ctx, container_t *container);
int container_pause(runtime_ctx_t *ctx, container_t *container);
int container_resume(runtime_ctx_t *ctx, container_t *container);

/* Container operations */
int container_exec(runtime_ctx_t *ctx, container_t *container, 
                   char *command, char **args, char **env);
int container_attach(runtime_ctx_t *ctx, container_t *container);
int container_logs(runtime_ctx_t *ctx, container_t *container, bool follow);

/* Container inspection */
int container_list(runtime_ctx_t *ctx, container_t ***containers, int *count);
int container_inspect(runtime_ctx_t *ctx, const char *id, container_t **container);
int container_stats(runtime_ctx_t *ctx, container_t *container);

/* Image management */
int image_pull(runtime_ctx_t *ctx, const char *name, const char *tag);
int image_build(runtime_ctx_t *ctx, const char *dockerfile, const char *tag);
int image_list(runtime_ctx_t *ctx, container_image_t ***images, int *count);
int image_remove(runtime_ctx_t *ctx, const char *id);
int image_export(runtime_ctx_t *ctx, const char *id, const char *path);
int image_import(runtime_ctx_t *ctx, const char *path, const char *tag);

/* Network management */
int network_create(runtime_ctx_t *ctx, const char *name, const char *subnet);
int network_delete(runtime_ctx_t *ctx, const char *name);
int network_connect(runtime_ctx_t *ctx, const char *network, container_t *container);
int network_disconnect(runtime_ctx_t *ctx, const char *network, container_t *container);

/* Error codes */
#define CR_OK 0
#define CR_ERROR -1
#define CR_ENOMEM -2
#define CR_EINVAL -3
#define CR_ENOTFOUND -4
#define CR_EEXIST -5
#define CR_EPERM -6
#define CR_ESYSCALL -7

/* Logging */
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} log_level_t;

void cr_log(log_level_t level, const char *fmt, ...);
void cr_set_log_level(log_level_t level);

#endif /* CRUNTIME_H */
