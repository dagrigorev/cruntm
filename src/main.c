#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include "../include/cruntime.h"

/* External utility functions */
extern void generate_id(char *buffer, size_t size);
extern int64_t parse_size(const char *str);

static void print_usage(const char *prog) {
    printf("CRuntime v%s - Production Container Runtime\n\n", CRUNTIME_VERSION);
    printf("Usage: %s [OPTIONS] COMMAND [ARGS...]\n\n", prog);
    printf("Commands:\n");
    printf("  run        Create and start a container\n");
    printf("  create     Create a container\n");
    printf("  start      Start a container\n");
    printf("  stop       Stop a container\n");
    printf("  kill       Kill a container\n");
    printf("  rm         Remove a container\n");
    printf("  ps         List containers\n");
    printf("  exec       Execute command in running container\n");
    printf("  pause      Pause a container\n");
    printf("  unpause    Resume a paused container\n");
    printf("  logs       View container logs\n");
    printf("  stats      Display container statistics\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help              Show this help message\n");
    printf("  -v, --version           Show version\n");
    printf("  -d, --runtime-dir DIR   Runtime directory (default: /var/run/cruntime)\n");
    printf("  --debug                 Enable debug logging\n");
}

static void print_version() {
    printf("cruntime version %s\n", CRUNTIME_VERSION);
}

/* Command: run */
static int cmd_run(runtime_ctx_t *ctx, int argc, char **argv) {
    /* Parse options */
    static struct option long_options[] = {
        {"name", required_argument, 0, 'n'},
        {"hostname", required_argument, 0, 'h'},
        {"memory", required_argument, 0, 'm'},
        {"cpus", required_argument, 0, 'c'},
        {"network", required_argument, 0, 0},
        {"ip", required_argument, 0, 0},
        {"publish", required_argument, 0, 'p'},
        {"volume", required_argument, 0, 'v'},
        {"env", required_argument, 0, 'e'},
        {"workdir", required_argument, 0, 'w'},
        {"detach", no_argument, 0, 'd'},
        {"rm", no_argument, 0, 0},
        {0, 0, 0, 0}
    };
    
    /* Initialize container config */
    container_config_t config;
    memset(&config, 0, sizeof(config));
    
    /* Generate container ID */
    generate_id(config.id, sizeof(config.id));
    
    /* Default configuration */
    config.namespaces.pid = true;
    config.namespaces.mnt = true;
    config.namespaces.uts = true;
    config.namespaces.ipc = true;
    config.namespaces.net = true;
    config.namespaces.cgroup = true;
    
    strcpy(config.hostname, "container");
    strcpy(config.network.bridge_name, "cr0");
    strcpy(config.network.subnet, "172.17.0.0/16");
    
    config.limits.memory_limit = -1;
    config.limits.cpu_shares = 1024;
    config.limits.cpu_quota = -1;
    config.limits.cpu_period = 100000;
    config.limits.pids_limit = -1;
    
    config.uid = getuid();
    config.gid = getgid();
    
    int detach = 0, auto_remove = 0;
    
    int opt, option_index = 0;
    while ((opt = getopt_long(argc, argv, "n:h:m:c:p:v:e:w:d", 
                              long_options, &option_index)) != -1) {
        switch (opt) {
            case 'n':
                strncpy(config.name, optarg, sizeof(config.name) - 1);
                break;
            case 'h':
                strncpy(config.hostname, optarg, sizeof(config.hostname) - 1);
                break;
            case 'm':
                config.limits.memory_limit = parse_size(optarg);
                break;
            case 'c':
                config.limits.cpu_quota = (int64_t)(atof(optarg) * 100000);
                break;
            case 'p': {
                /* Parse port mapping: host:container or host:container/proto */
                char *colon = strchr(optarg, ':');
                if (colon && config.network.num_ports < MAX_PORTS) {
                    *colon = '\0';
                    port_mapping_t *pm = &config.network.ports[config.network.num_ports++];
                    pm->host_port = atoi(optarg);
                    
                    char *slash = strchr(colon + 1, '/');
                    if (slash) {
                        *slash = '\0';
                        pm->container_port = atoi(colon + 1);
                        strncpy(pm->protocol, slash + 1, sizeof(pm->protocol) - 1);
                    } else {
                        pm->container_port = atoi(colon + 1);
                        strcpy(pm->protocol, "tcp");
                    }
                }
                break;
            }
            case 'v': {
                /* Parse volume: host:container */
                if (config.num_mounts < MAX_MOUNTS) {
                    char *colon = strchr(optarg, ':');
                    if (colon) {
                        *colon = '\0';
                        mount_point_t *mp = &config.mounts[config.num_mounts++];
                        strncpy(mp->source, optarg, sizeof(mp->source) - 1);
                        strncpy(mp->target, colon + 1, sizeof(mp->target) - 1);
                        strcpy(mp->fstype, "bind");
                        mp->flags = MS_BIND;
                    }
                }
                break;
            }
            case 'e':
                if (config.num_env < MAX_ENV_VARS) {
                    config.env[config.num_env++] = strdup(optarg);
                }
                break;
            case 'w':
                strncpy(config.working_dir, optarg, sizeof(config.working_dir) - 1);
                break;
            case 'd':
                detach = 1;
                break;
            case 0:
                if (strcmp(long_options[option_index].name, "rm") == 0) {
                    auto_remove = 1;
                } else if (strcmp(long_options[option_index].name, "network") == 0) {
                    strncpy(config.network.bridge_name, optarg, 
                           sizeof(config.network.bridge_name) - 1);
                } else if (strcmp(long_options[option_index].name, "ip") == 0) {
                    strncpy(config.network.ip_address, optarg,
                           sizeof(config.network.ip_address) - 1);
                }
                break;
            default:
                return 1;
        }
    }
    
    if (optind >= argc) {
        fprintf(stderr, "Error: missing image name\n");
        return 1;
    }
    
    /* For now, use image name as rootfs path */
    /* TODO: Implement proper image pulling and extraction */
    const char *image = argv[optind++];
    snprintf(config.rootfs, sizeof(config.rootfs), "%s/images/%s/rootfs", 
             ctx->image_dir, image);
    
    /* Get command and args */
    if (optind < argc) {
        config.command = argv[optind++];
        config.args = &argv[optind - 1];
        config.num_args = argc - optind + 1;
    } else {
        config.command = "/bin/sh";
    }
    
    /* Create and start container */
    container_t *container;
    if (container_create(ctx, &config, &container) != CR_OK) {
        fprintf(stderr, "Failed to create container\n");
        return 1;
    }
    
    printf("%s\n", config.id);
    
    if (container_start(ctx, container) != CR_OK) {
        fprintf(stderr, "Failed to start container\n");
        container_delete(ctx, container);
        return 1;
    }
    
    if (!detach) {
        /* Wait for container to exit */
        int status;
        waitpid(container->init_pid, &status, 0);
        
        if (auto_remove) {
            container_delete(ctx, container);
        }
    }
    
    return 0;
}

/* Command: ps */
static int cmd_ps(runtime_ctx_t *ctx, int argc, char **argv) {
    printf("CONTAINER ID    NAME              STATUS     PID\n");
    
    /* TODO: Load container states from state_dir and display */
    printf("(container listing not yet implemented)\n");
    
    return 0;
}

int main(int argc, char **argv) {
    /* Parse global options */
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {"runtime-dir", required_argument, 0, 'd'},
        {"debug", no_argument, 0, 0},
        {0, 0, 0, 0}
    };
    
    /* Initialize runtime context */
    runtime_ctx_t ctx;
    strcpy(ctx.runtime_dir, "/var/run/cruntime");
    strcpy(ctx.state_dir, "/var/lib/cruntime/state");
    strcpy(ctx.image_dir, "/var/lib/cruntime/images");
    strcpy(ctx.network_dir, "/var/run/cruntime/netns");
    ctx.use_systemd = false;
    
    int opt, option_index = 0;
    while ((opt = getopt_long(argc, argv, "+hvd:", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv[0]);
                return 0;
            case 'v':
                print_version();
                return 0;
            case 'd':
                strncpy(ctx.runtime_dir, optarg, sizeof(ctx.runtime_dir) - 1);
                break;
            case 0:
                if (strcmp(long_options[option_index].name, "debug") == 0) {
                    cr_set_log_level(LOG_DEBUG);
                }
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    if (optind >= argc) {
        print_usage(argv[0]);
        return 1;
    }
    
    /* Check for root privileges */
    if (geteuid() != 0) {
        fprintf(stderr, "Error: cruntime requires root privileges\n");
        return 1;
    }
    
    /* Initialize runtime */
    if (runtime_init(&ctx) != CR_OK) {
        fprintf(stderr, "Failed to initialize runtime\n");
        return 1;
    }
    
    /* Get command */
    const char *cmd = argv[optind];
    optind++;
    
    int ret = 0;
    if (strcmp(cmd, "run") == 0) {
        ret = cmd_run(&ctx, argc, argv);
    } else if (strcmp(cmd, "ps") == 0) {
        ret = cmd_ps(&ctx, argc, argv);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        ret = 1;
    }
    
    runtime_cleanup(&ctx);
    return ret;
}
