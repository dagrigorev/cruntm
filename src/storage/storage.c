#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include "../include/cruntime.h"

/* Create directory recursively */
static int mkdir_recursive(const char *path, mode_t mode) {
    char tmp[MAX_PATH_LEN];
    char *p = NULL;
    size_t len;
    
    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }
    
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, mode) < 0 && errno != EEXIST) {
                return CR_ESYSCALL;
            }
            *p = '/';
        }
    }
    
    if (mkdir(tmp, mode) < 0 && errno != EEXIST) {
        return CR_ESYSCALL;
    }
    
    return CR_OK;
}

/* Initialize storage driver */
int storage_init(runtime_ctx_t *ctx) {
    char path[MAX_PATH_LEN];
    
    /* Create base directories */
    snprintf(path, sizeof(path), "%s/overlay", ctx->image_dir);
    mkdir_recursive(path, 0755);
    
    snprintf(path, sizeof(path), "%s/layers", ctx->image_dir);
    mkdir_recursive(path, 0755);
    
    snprintf(path, sizeof(path), "%s/images", ctx->image_dir);
    mkdir_recursive(path, 0755);
    
    cr_log(LOG_INFO, "Initialized storage driver at %s", ctx->image_dir);
    return CR_OK;
}

/* Create layer from directory */
int storage_create_layer(runtime_ctx_t *ctx, const char *source_dir, 
                         const char *layer_id, char *layer_path, size_t path_len) {
    char cmd[MAX_PATH_LEN * 2];
    
    /* Create layer directory */
    snprintf(layer_path, path_len, "%s/layers/%s", ctx->image_dir, layer_id);
    if (mkdir_recursive(layer_path, 0755) != CR_OK) {
        return CR_ERROR;
    }
    
    /* Copy contents to layer */
    snprintf(cmd, sizeof(cmd), "cp -a %s/* %s/ 2>/dev/null || true", 
             source_dir, layer_path);
    if (system(cmd) != 0) {
        cr_log(LOG_WARN, "Layer copy completed with warnings");
    }
    
    cr_log(LOG_DEBUG, "Created layer %s at %s", layer_id, layer_path);
    return CR_OK;
}

/* Mount overlay filesystem */
int storage_mount_overlay(runtime_ctx_t *ctx, container_image_t *image,
                          const char *container_id, char *mount_point, size_t mp_len) {
    char lowerdir[MAX_PATH_LEN * 4] = "";
    char upperdir[MAX_PATH_LEN];
    char workdir[MAX_PATH_LEN];
    char merged[MAX_PATH_LEN];
    char options[MAX_PATH_LEN * 5];
    
    /* Create overlay workspace for this container */
    snprintf(upperdir, sizeof(upperdir), "%s/overlay/%s/upper", 
             ctx->image_dir, container_id);
    snprintf(workdir, sizeof(workdir), "%s/overlay/%s/work", 
             ctx->image_dir, container_id);
    snprintf(merged, sizeof(merged), "%s/overlay/%s/merged", 
             ctx->image_dir, container_id);
    
    mkdir_recursive(upperdir, 0755);
    mkdir_recursive(workdir, 0755);
    mkdir_recursive(merged, 0755);
    
    /* Build lowerdir string (bottom layer first) */
    for (int i = image->num_layers - 1; i >= 0; i--) {
        if (strlen(lowerdir) > 0) {
            strncat(lowerdir, ":", sizeof(lowerdir) - strlen(lowerdir) - 1);
        }
        strncat(lowerdir, image->layers[i].diff_path, 
                sizeof(lowerdir) - strlen(lowerdir) - 1);
    }
    
    /* Build mount options */
    snprintf(options, sizeof(options), 
             "lowerdir=%s,upperdir=%s,workdir=%s",
             lowerdir, upperdir, workdir);
    
    /* Mount overlay */
    if (mount("overlay", merged, "overlay", 0, options) < 0) {
        cr_log(LOG_ERROR, "Failed to mount overlay: %s", strerror(errno));
        cr_log(LOG_ERROR, "Options: %s", options);
        return CR_ESYSCALL;
    }
    
    strncpy(mount_point, merged, mp_len);
    cr_log(LOG_INFO, "Mounted overlay filesystem at %s", merged);
    return CR_OK;
}

/* Unmount overlay filesystem */
int storage_unmount_overlay(const char *mount_point) {
    if (umount2(mount_point, MNT_DETACH) < 0) {
        if (errno != EINVAL && errno != ENOENT) {
            cr_log(LOG_ERROR, "Failed to unmount overlay %s: %s", 
                   mount_point, strerror(errno));
            return CR_ESYSCALL;
        }
    }
    
    cr_log(LOG_DEBUG, "Unmounted overlay filesystem at %s", mount_point);
    return CR_OK;
}

/* Remove overlay workspace */
int storage_remove_overlay(runtime_ctx_t *ctx, const char *container_id) {
    char path[MAX_PATH_LEN];
    char cmd[MAX_PATH_LEN * 2];
    
    /* Unmount if still mounted */
    snprintf(path, sizeof(path), "%s/overlay/%s/merged", ctx->image_dir, container_id);
    storage_unmount_overlay(path);
    
    /* Remove overlay directory */
    snprintf(path, sizeof(path), "%s/overlay/%s", ctx->image_dir, container_id);
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);
    
    cr_log(LOG_DEBUG, "Removed overlay workspace for %s", container_id);
    return CR_OK;
}

/* Get layer size */
static size_t get_directory_size(const char *path) {
    char cmd[MAX_PATH_LEN];
    FILE *fp;
    size_t size = 0;
    
    snprintf(cmd, sizeof(cmd), "du -sb %s 2>/dev/null | cut -f1", path);
    fp = popen(cmd, "r");
    if (fp) {
        fscanf(fp, "%zu", &size);
        pclose(fp);
    }
    
    return size;
}

/* Calculate layer diff */
int storage_create_diff_layer(runtime_ctx_t *ctx, const char *lower_layer,
                               const char *upper_layer, const char *layer_id,
                               char *diff_path, size_t path_len) {
    char cmd[MAX_PATH_LEN * 3];
    
    /* Create diff directory */
    snprintf(diff_path, path_len, "%s/layers/%s", ctx->image_dir, layer_id);
    mkdir_recursive(diff_path, 0755);
    
    /* Use rsync to find differences */
    snprintf(cmd, sizeof(cmd),
             "rsync -a --delete --link-dest=%s/ %s/ %s/",
             lower_layer, upper_layer, diff_path);
    
    if (system(cmd) != 0) {
        cr_log(LOG_ERROR, "Failed to create diff layer");
        return CR_ERROR;
    }
    
    cr_log(LOG_DEBUG, "Created diff layer %s", layer_id);
    return CR_OK;
}

/* Export layer as tarball */
int storage_export_layer(const char *layer_path, const char *output_tar) {
    char cmd[MAX_PATH_LEN * 2];
    
    snprintf(cmd, sizeof(cmd),
             "tar -czf %s -C %s .",
             output_tar, layer_path);
    
    if (system(cmd) != 0) {
        cr_log(LOG_ERROR, "Failed to export layer");
        return CR_ERROR;
    }
    
    cr_log(LOG_DEBUG, "Exported layer to %s", output_tar);
    return CR_OK;
}

/* Import layer from tarball */
int storage_import_layer(runtime_ctx_t *ctx, const char *input_tar,
                         const char *layer_id, char *layer_path, size_t path_len) {
    char cmd[MAX_PATH_LEN * 2];
    
    /* Create layer directory */
    snprintf(layer_path, path_len, "%s/layers/%s", ctx->image_dir, layer_id);
    mkdir_recursive(layer_path, 0755);
    
    /* Extract tarball */
    snprintf(cmd, sizeof(cmd),
             "tar -xzf %s -C %s",
             input_tar, layer_path);
    
    if (system(cmd) != 0) {
        cr_log(LOG_ERROR, "Failed to import layer");
        return CR_ERROR;
    }
    
    cr_log(LOG_DEBUG, "Imported layer %s from %s", layer_id, input_tar);
    return CR_OK;
}

/* Commit container changes to new layer */
int storage_commit_container(runtime_ctx_t *ctx, const char *container_id,
                              const char *layer_id) {
    char upperdir[MAX_PATH_LEN];
    char layer_path[MAX_PATH_LEN];
    char cmd[MAX_PATH_LEN * 2];
    
    /* Get container's upper directory (contains changes) */
    snprintf(upperdir, sizeof(upperdir), "%s/overlay/%s/upper",
             ctx->image_dir, container_id);
    
    /* Create new layer from changes */
    snprintf(layer_path, sizeof(layer_path), "%s/layers/%s",
             ctx->image_dir, layer_id);
    mkdir_recursive(layer_path, 0755);
    
    /* Copy changes to new layer */
    snprintf(cmd, sizeof(cmd), "cp -a %s/* %s/ 2>/dev/null || true",
             upperdir, layer_path);
    system(cmd);
    
    cr_log(LOG_INFO, "Committed container %s changes to layer %s",
           container_id, layer_id);
    return CR_OK;
}

/* Get storage statistics */
int storage_get_stats(runtime_ctx_t *ctx, char *stats_buffer, size_t buffer_size) {
    char path[MAX_PATH_LEN];
    int offset = 0;
    
    /* Count layers */
    snprintf(path, sizeof(path), "%s/layers", ctx->image_dir);
    DIR *dir = opendir(path);
    int layer_count = 0;
    size_t total_size = 0;
    
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            layer_count++;
            
            char layer_path[MAX_PATH_LEN];
            snprintf(layer_path, sizeof(layer_path), "%s/%s", path, ent->d_name);
            total_size += get_directory_size(layer_path);
        }
        closedir(dir);
    }
    
    offset += snprintf(stats_buffer + offset, buffer_size - offset,
                      "Layers: %d\n", layer_count);
    offset += snprintf(stats_buffer + offset, buffer_size - offset,
                      "Total Size: %.2f MB\n", total_size / (1024.0 * 1024.0));
    
    return CR_OK;
}
