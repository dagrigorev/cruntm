#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "../include/cruntime.h"

#define CGROUP_ROOT "/sys/fs/cgroup"
#define CGROUP_CONTAINER_PREFIX "cruntime"

/* Write value to cgroup file */
static int cgroup_write_file(const char *path, const char *value) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        cr_log(LOG_ERROR, "Failed to open cgroup file %s: %s", path, strerror(errno));
        return CR_ESYSCALL;
    }
    
    ssize_t len = strlen(value);
    if (write(fd, value, len) != len) {
        cr_log(LOG_ERROR, "Failed to write to %s: %s", path, strerror(errno));
        close(fd);
        return CR_ESYSCALL;
    }
    
    close(fd);
    return CR_OK;
}

/* Read value from cgroup file */
static int cgroup_read_file(const char *path, char *buffer, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return CR_ESYSCALL;
    }
    
    ssize_t bytes = read(fd, buffer, size - 1);
    close(fd);
    
    if (bytes < 0) {
        return CR_ESYSCALL;
    }
    
    buffer[bytes] = '\0';
    return CR_OK;
}

/* Check if using cgroups v2 */
static bool is_cgroup_v2(void) {
    struct stat st;
    return (stat("/sys/fs/cgroup/cgroup.controllers", &st) == 0);
}

/* Create cgroup hierarchy for container */
int cgroup_create(const char *container_id, char *cgroup_path, size_t path_len) {
    if (!is_cgroup_v2()) {
        cr_log(LOG_ERROR, "cgroups v2 required but not available");
        return CR_ERROR;
    }
    
    /* Create cgroup path: /sys/fs/cgroup/cruntime/<container_id> */
    snprintf(cgroup_path, path_len, "%s/%s/%s", 
             CGROUP_ROOT, CGROUP_CONTAINER_PREFIX, container_id);
    
    /* Create directory structure */
    char parent_path[MAX_PATH_LEN];
    snprintf(parent_path, sizeof(parent_path), "%s/%s", 
             CGROUP_ROOT, CGROUP_CONTAINER_PREFIX);
    mkdir(parent_path, 0755);
    
    if (mkdir(cgroup_path, 0755) < 0 && errno != EEXIST) {
        cr_log(LOG_ERROR, "Failed to create cgroup directory %s: %s", 
               cgroup_path, strerror(errno));
        return CR_ESYSCALL;
    }
    
    /* Enable controllers in parent cgroup */
    char subtree_path[MAX_PATH_LEN];
    snprintf(subtree_path, sizeof(subtree_path), "%s/cgroup.subtree_control", parent_path);
    cgroup_write_file(subtree_path, "+cpu +memory +io +pids");
    
    cr_log(LOG_DEBUG, "Created cgroup at %s", cgroup_path);
    return CR_OK;
}

/* Apply resource limits to cgroup */
int cgroup_apply_limits(const char *cgroup_path, resource_limits_t *limits) {
    char file_path[MAX_PATH_LEN];
    char value[256];
    
    /* Memory limit */
    if (limits->memory_limit > 0) {
        snprintf(file_path, sizeof(file_path), "%s/memory.max", cgroup_path);
        snprintf(value, sizeof(value), "%ld", limits->memory_limit);
        if (cgroup_write_file(file_path, value) != CR_OK) {
            cr_log(LOG_WARN, "Failed to set memory limit");
        } else {
            cr_log(LOG_DEBUG, "Set memory limit to %ld bytes", limits->memory_limit);
        }
    }
    
    /* Memory + swap limit */
    if (limits->memory_swap > 0) {
        snprintf(file_path, sizeof(file_path), "%s/memory.swap.max", cgroup_path);
        snprintf(value, sizeof(value), "%ld", limits->memory_swap);
        cgroup_write_file(file_path, value);
    }
    
    /* CPU weight (shares) - cgroups v2 uses weight instead of shares */
    if (limits->cpu_shares > 0) {
        snprintf(file_path, sizeof(file_path), "%s/cpu.weight", cgroup_path);
        /* Convert shares (2-262144) to weight (1-10000) */
        long weight = (limits->cpu_shares * 10000) / 262144;
        if (weight < 1) weight = 1;
        if (weight > 10000) weight = 10000;
        snprintf(value, sizeof(value), "%ld", weight);
        if (cgroup_write_file(file_path, value) != CR_OK) {
            cr_log(LOG_WARN, "Failed to set CPU weight");
        } else {
            cr_log(LOG_DEBUG, "Set CPU weight to %ld", weight);
        }
    }
    
    /* CPU quota */
    if (limits->cpu_quota > 0 && limits->cpu_period > 0) {
        snprintf(file_path, sizeof(file_path), "%s/cpu.max", cgroup_path);
        snprintf(value, sizeof(value), "%ld %ld", limits->cpu_quota, limits->cpu_period);
        if (cgroup_write_file(file_path, value) != CR_OK) {
            cr_log(LOG_WARN, "Failed to set CPU quota");
        } else {
            cr_log(LOG_DEBUG, "Set CPU quota %ld/%ld", limits->cpu_quota, limits->cpu_period);
        }
    }
    
    /* PIDs limit */
    if (limits->pids_limit > 0) {
        snprintf(file_path, sizeof(file_path), "%s/pids.max", cgroup_path);
        snprintf(value, sizeof(value), "%ld", limits->pids_limit);
        if (cgroup_write_file(file_path, value) != CR_OK) {
            cr_log(LOG_WARN, "Failed to set PIDs limit");
        } else {
            cr_log(LOG_DEBUG, "Set PIDs limit to %ld", limits->pids_limit);
        }
    }
    
    /* Block I/O weight */
    if (limits->blkio_weight > 0) {
        snprintf(file_path, sizeof(file_path), "%s/io.weight", cgroup_path);
        snprintf(value, sizeof(value), "default %ld", limits->blkio_weight);
        if (cgroup_write_file(file_path, value) != CR_OK) {
            cr_log(LOG_WARN, "Failed to set I/O weight");
        } else {
            cr_log(LOG_DEBUG, "Set I/O weight to %ld", limits->blkio_weight);
        }
    }
    
    return CR_OK;
}

/* Add process to cgroup */
int cgroup_add_process(const char *cgroup_path, pid_t pid) {
    char procs_path[MAX_PATH_LEN];
    char pid_str[32];
    
    snprintf(procs_path, sizeof(procs_path), "%s/cgroup.procs", cgroup_path);
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    
    if (cgroup_write_file(procs_path, pid_str) != CR_OK) {
        cr_log(LOG_ERROR, "Failed to add process %d to cgroup", pid);
        return CR_ESYSCALL;
    }
    
    cr_log(LOG_DEBUG, "Added process %d to cgroup %s", pid, cgroup_path);
    return CR_OK;
}

/* Get cgroup statistics */
int cgroup_get_stats(const char *cgroup_path, char *stats_buffer, size_t buffer_size) {
    char file_path[MAX_PATH_LEN];
    char value[256];
    int offset = 0;
    
    /* Memory usage */
    snprintf(file_path, sizeof(file_path), "%s/memory.current", cgroup_path);
    if (cgroup_read_file(file_path, value, sizeof(value)) == CR_OK) {
        offset += snprintf(stats_buffer + offset, buffer_size - offset,
                          "Memory: %s bytes\n", value);
    }
    
    /* Memory limit */
    snprintf(file_path, sizeof(file_path), "%s/memory.max", cgroup_path);
    if (cgroup_read_file(file_path, value, sizeof(value)) == CR_OK) {
        offset += snprintf(stats_buffer + offset, buffer_size - offset,
                          "Memory Limit: %s\n", value);
    }
    
    /* CPU usage */
    snprintf(file_path, sizeof(file_path), "%s/cpu.stat", cgroup_path);
    if (cgroup_read_file(file_path, value, sizeof(value)) == CR_OK) {
        offset += snprintf(stats_buffer + offset, buffer_size - offset,
                          "CPU Stats: %s", value);
    }
    
    /* PID count */
    snprintf(file_path, sizeof(file_path), "%s/pids.current", cgroup_path);
    if (cgroup_read_file(file_path, value, sizeof(value)) == CR_OK) {
        offset += snprintf(stats_buffer + offset, buffer_size - offset,
                          "PIDs: %s\n", value);
    }
    
    return CR_OK;
}

/* Freeze cgroup (pause container) */
int cgroup_freeze(const char *cgroup_path) {
    char freeze_path[MAX_PATH_LEN];
    snprintf(freeze_path, sizeof(freeze_path), "%s/cgroup.freeze", cgroup_path);
    
    if (cgroup_write_file(freeze_path, "1") != CR_OK) {
        cr_log(LOG_ERROR, "Failed to freeze cgroup");
        return CR_ESYSCALL;
    }
    
    cr_log(LOG_DEBUG, "Froze cgroup %s", cgroup_path);
    return CR_OK;
}

/* Unfreeze cgroup (resume container) */
int cgroup_unfreeze(const char *cgroup_path) {
    char freeze_path[MAX_PATH_LEN];
    snprintf(freeze_path, sizeof(freeze_path), "%s/cgroup.freeze", cgroup_path);
    
    if (cgroup_write_file(freeze_path, "0") != CR_OK) {
        cr_log(LOG_ERROR, "Failed to unfreeze cgroup");
        return CR_ESYSCALL;
    }
    
    cr_log(LOG_DEBUG, "Unfroze cgroup %s", cgroup_path);
    return CR_OK;
}

/* Kill all processes in cgroup */
int cgroup_kill_all(const char *cgroup_path) {
    char kill_path[MAX_PATH_LEN];
    snprintf(kill_path, sizeof(kill_path), "%s/cgroup.kill", cgroup_path);
    
    if (cgroup_write_file(kill_path, "1") != CR_OK) {
        cr_log(LOG_ERROR, "Failed to kill cgroup processes");
        return CR_ESYSCALL;
    }
    
    cr_log(LOG_DEBUG, "Killed all processes in cgroup %s", cgroup_path);
    return CR_OK;
}

/* Destroy cgroup */
int cgroup_destroy(const char *cgroup_path) {
    /* Kill all processes first */
    cgroup_kill_all(cgroup_path);
    
    /* Wait a bit for processes to terminate */
    usleep(100000); /* 100ms */
    
    /* Remove directory */
    if (rmdir(cgroup_path) < 0 && errno != ENOENT) {
        cr_log(LOG_ERROR, "Failed to remove cgroup %s: %s", 
               cgroup_path, strerror(errno));
        return CR_ESYSCALL;
    }
    
    cr_log(LOG_DEBUG, "Destroyed cgroup %s", cgroup_path);
    return CR_OK;
}
