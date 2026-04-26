#include <sched.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/syscall.h>
#include "../include/cruntime.h"

/* Get namespace flags from config */
static int get_clone_flags(namespace_config_t *ns_config) {
    int flags = 0;
    
    if (ns_config->pid) flags |= CLONE_NEWPID;
    if (ns_config->net) flags |= CLONE_NEWNET;
    if (ns_config->mnt) flags |= CLONE_NEWNS;
    if (ns_config->uts) flags |= CLONE_NEWUTS;
    if (ns_config->ipc) flags |= CLONE_NEWIPC;
    if (ns_config->user) flags |= CLONE_NEWUSER;
    if (ns_config->cgroup) flags |= CLONE_NEWCGROUP;
    
    return flags;
}

/* Setup mount namespace */
int namespace_setup_mounts(container_config_t *config) {
    /* Make everything private to prevent mount propagation */
    if (mount(NULL, "/", NULL, MS_PRIVATE | MS_REC, NULL) < 0) {
        cr_log(LOG_ERROR, "Failed to make / private: %s", strerror(errno));
        return CR_ESYSCALL;
    }
    
    /* Create new rootfs mount */
    if (mount(config->rootfs, config->rootfs, NULL, MS_BIND | MS_REC, NULL) < 0) {
        cr_log(LOG_ERROR, "Failed to bind mount rootfs: %s", strerror(errno));
        return CR_ESYSCALL;
    }
    
    /* Make rootfs readonly if requested */
    if (config->readonly_rootfs) {
        if (mount(NULL, config->rootfs, NULL, MS_BIND | MS_REMOUNT | MS_RDONLY, NULL) < 0) {
            cr_log(LOG_ERROR, "Failed to make rootfs readonly: %s", strerror(errno));
            return CR_ESYSCALL;
        }
    }
    
    /* Mount proc */
    char proc_path[MAX_PATH_LEN];
    snprintf(proc_path, sizeof(proc_path), "%s/proc", config->rootfs);
    mkdir(proc_path, 0755);
    if (mount("proc", proc_path, "proc", 0, NULL) < 0) {
        cr_log(LOG_WARN, "Failed to mount /proc: %s", strerror(errno));
    }
    
    /* Mount sysfs */
    char sys_path[MAX_PATH_LEN];
    snprintf(sys_path, sizeof(sys_path), "%s/sys", config->rootfs);
    mkdir(sys_path, 0755);
    if (mount("sysfs", sys_path, "sysfs", 0, NULL) < 0) {
        cr_log(LOG_WARN, "Failed to mount /sys: %s", strerror(errno));
    }
    
    /* Mount devtmpfs */
    char dev_path[MAX_PATH_LEN];
    snprintf(dev_path, sizeof(dev_path), "%s/dev", config->rootfs);
    mkdir(dev_path, 0755);
    if (mount("devtmpfs", dev_path, "devtmpfs", MS_NOSUID | MS_STRICTATIME, "mode=755") < 0) {
        cr_log(LOG_WARN, "Failed to mount /dev: %s", strerror(errno));
    }
    
    /* Mount devpts for pseudo-terminals */
    char devpts_path[MAX_PATH_LEN];
    snprintf(devpts_path, sizeof(devpts_path), "%s/dev/pts", config->rootfs);
    mkdir(devpts_path, 0755);
    if (mount("devpts", devpts_path, "devpts", 0, "newinstance,ptmxmode=0666") < 0) {
        cr_log(LOG_WARN, "Failed to mount /dev/pts: %s", strerror(errno));
    }
    
    /* Mount tmpfs for /tmp and /run */
    char tmp_path[MAX_PATH_LEN];
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", config->rootfs);
    mkdir(tmp_path, 01777);
    mount("tmpfs", tmp_path, "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777");
    
    char run_path[MAX_PATH_LEN];
    snprintf(run_path, sizeof(run_path), "%s/run", config->rootfs);
    mkdir(run_path, 0755);
    mount("tmpfs", run_path, "tmpfs", MS_NOSUID | MS_NODEV | MS_NOEXEC, "mode=755");
    
    /* Mount additional volumes */
    for (int i = 0; i < config->num_mounts; i++) {
        mount_point_t *mp = &config->mounts[i];
        char target[MAX_PATH_LEN];
        snprintf(target, sizeof(target), "%s%s", config->rootfs, mp->target);
        
        /* Create target directory */
        mkdir(target, 0755);
        
        if (mount(mp->source, target, mp->fstype, mp->flags, mp->options) < 0) {
            cr_log(LOG_ERROR, "Failed to mount %s to %s: %s", 
                   mp->source, target, strerror(errno));
            return CR_ESYSCALL;
        }
        
        cr_log(LOG_DEBUG, "Mounted %s to %s", mp->source, target);
    }
    
    return CR_OK;
}

/* Pivot root to new rootfs */
int namespace_pivot_root(const char *new_root) {
    char old_root[MAX_PATH_LEN];
    snprintf(old_root, sizeof(old_root), "%s/.oldroot", new_root);
    
    /* Create directory for old root */
    mkdir(old_root, 0700);
    
    /* Pivot root */
    if (syscall(SYS_pivot_root, new_root, old_root) < 0) {
        cr_log(LOG_ERROR, "pivot_root failed: %s", strerror(errno));
        return CR_ESYSCALL;
    }
    
    /* Change to new root */
    if (chdir("/") < 0) {
        cr_log(LOG_ERROR, "chdir(/) failed: %s", strerror(errno));
        return CR_ESYSCALL;
    }
    
    /* Unmount old root */
    if (umount2("/.oldroot", MNT_DETACH) < 0) {
        cr_log(LOG_ERROR, "umount2(/.oldroot) failed: %s", strerror(errno));
        return CR_ESYSCALL;
    }
    
    /* Remove old root directory */
    rmdir("/.oldroot");
    
    return CR_OK;
}

/* Setup UTS namespace (hostname) */
int namespace_setup_uts(const char *hostname) {
    if (sethostname(hostname, strlen(hostname)) < 0) {
        cr_log(LOG_ERROR, "sethostname failed: %s", strerror(errno));
        return CR_ESYSCALL;
    }
    
    cr_log(LOG_DEBUG, "Set hostname to %s", hostname);
    return CR_OK;
}

/* Setup user namespace mappings */
int namespace_setup_user(uid_t uid, gid_t gid) {
    char map_path[256];
    char map_data[256];
    int fd;
    
    /* Deny setgroups first */
    fd = open("/proc/self/setgroups", O_WRONLY);
    if (fd >= 0) {
        write(fd, "deny", 4);
        close(fd);
    }
    
    /* Map UID */
    sprintf(map_path, "/proc/self/uid_map");
    sprintf(map_data, "0 %d 1", uid);
    
    fd = open(map_path, O_WRONLY);
    if (fd < 0) {
        cr_log(LOG_ERROR, "Failed to open uid_map: %s", strerror(errno));
        return CR_ESYSCALL;
    }
    
    if (write(fd, map_data, strlen(map_data)) < 0) {
        cr_log(LOG_ERROR, "Failed to write uid_map: %s", strerror(errno));
        close(fd);
        return CR_ESYSCALL;
    }
    close(fd);
    
    /* Map GID */
    sprintf(map_path, "/proc/self/gid_map");
    sprintf(map_data, "0 %d 1", gid);
    
    fd = open(map_path, O_WRONLY);
    if (fd < 0) {
        cr_log(LOG_ERROR, "Failed to open gid_map: %s", strerror(errno));
        return CR_ESYSCALL;
    }
    
    if (write(fd, map_data, strlen(map_data)) < 0) {
        cr_log(LOG_ERROR, "Failed to write gid_map: %s", strerror(errno));
        close(fd);
        return CR_ESYSCALL;
    }
    close(fd);
    
    cr_log(LOG_DEBUG, "Mapped UID %d and GID %d to container root", uid, gid);
    return CR_OK;
}

/* Join existing namespace */
int namespace_join(const char *ns_path, int ns_type) {
    int fd = open(ns_path, O_RDONLY);
    if (fd < 0) {
        cr_log(LOG_ERROR, "Failed to open namespace %s: %s", ns_path, strerror(errno));
        return CR_ESYSCALL;
    }
    
    if (setns(fd, ns_type) < 0) {
        cr_log(LOG_ERROR, "Failed to join namespace %s: %s", ns_path, strerror(errno));
        close(fd);
        return CR_ESYSCALL;
    }
    
    close(fd);
    return CR_OK;
}

/* Persist namespace to filesystem */
int namespace_persist(int pid, const char *ns_name, const char *persist_path) {
    char src[256];
    snprintf(src, sizeof(src), "/proc/%d/ns/%s", pid, ns_name);
    
    /* Create parent directory if needed */
    char *dir = strdup(persist_path);
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        mkdir(dir, 0755);
    }
    free(dir);
    
    /* Create bind mount to persist namespace */
    int fd = open(persist_path, O_RDONLY | O_CREAT | O_EXCL, 0444);
    if (fd < 0) {
        cr_log(LOG_ERROR, "Failed to create namespace persist file: %s", strerror(errno));
        return CR_ESYSCALL;
    }
    close(fd);
    
    if (mount(src, persist_path, NULL, MS_BIND, NULL) < 0) {
        cr_log(LOG_ERROR, "Failed to bind mount namespace: %s", strerror(errno));
        unlink(persist_path);
        return CR_ESYSCALL;
    }
    
    cr_log(LOG_DEBUG, "Persisted namespace %s to %s", ns_name, persist_path);
    return CR_OK;
}

/* Get clone flags needed for container */
int namespace_get_clone_flags(namespace_config_t *config) {
    return get_clone_flags(config);
}
