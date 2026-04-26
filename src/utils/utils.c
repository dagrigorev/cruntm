#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include "../include/cruntime.h"

static log_level_t current_log_level = LOG_INFO;

/* ANSI color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_GRAY    "\033[90m"

static const char* level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

static const char* level_colors[] = {
    COLOR_GRAY,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_RED,
    COLOR_RED
};

/* Set log level */
void cr_set_log_level(log_level_t level) {
    current_log_level = level;
}

/* Get current timestamp */
static void get_timestamp(char *buffer, size_t size) {
    struct timeval tv;
    struct tm *tm_info;
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
    snprintf(buffer + strlen(buffer), size - strlen(buffer), ".%03ld", tv.tv_usec / 1000);
}

/* Log message */
void cr_log(log_level_t level, const char *fmt, ...) {
    if (level < current_log_level) {
        return;
    }
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    /* Determine if output is to a terminal */
    int use_color = isatty(STDERR_FILENO);
    
    /* Print timestamp and level */
    if (use_color) {
        fprintf(stderr, "%s[%s] %s%-5s%s ", 
                COLOR_GRAY, timestamp,
                level_colors[level], level_strings[level],
                COLOR_RESET);
    } else {
        fprintf(stderr, "[%s] %-5s ", timestamp, level_strings[level]);
    }
    
    /* Print message */
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
    fflush(stderr);
    
    /* Exit on fatal errors */
    if (level == LOG_FATAL) {
        exit(1);
    }
}

/* Generate random ID */
void generate_id(char *buffer, size_t size) {
    const char charset[] = "0123456789abcdef";
    
    FILE *f = fopen("/dev/urandom", "r");
    if (!f) {
        /* Fallback to pseudo-random */
        srand(time(NULL) ^ getpid());
        for (size_t i = 0; i < size - 1; i++) {
            buffer[i] = charset[rand() % 16];
        }
    } else {
        unsigned char random_bytes[32];
        fread(random_bytes, 1, sizeof(random_bytes), f);
        fclose(f);
        
        for (size_t i = 0; i < size - 1 && i < sizeof(random_bytes); i++) {
            buffer[i] = charset[random_bytes[i] % 16];
        }
    }
    
    buffer[size - 1] = '\0';
}

/* Parse size string (e.g., "100M", "1G") */
int64_t parse_size(const char *str) {
    char *endptr;
    double value = strtod(str, &endptr);
    
    if (endptr == str) {
        return -1;
    }
    
    int64_t multiplier = 1;
    if (*endptr != '\0') {
        switch (*endptr) {
            case 'K': case 'k': multiplier = 1024; break;
            case 'M': case 'm': multiplier = 1024 * 1024; break;
            case 'G': case 'g': multiplier = 1024 * 1024 * 1024; break;
            case 'T': case 't': multiplier = 1024LL * 1024 * 1024 * 1024; break;
            default: return -1;
        }
    }
    
    return (int64_t)(value * multiplier);
}

/* Format size for display */
void format_size(int64_t bytes, char *buffer, size_t size) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double value = (double)bytes;
    
    while (value >= 1024 && unit_idx < 4) {
        value /= 1024;
        unit_idx++;
    }
    
    if (unit_idx == 0) {
        snprintf(buffer, size, "%ld %s", bytes, units[unit_idx]);
    } else {
        snprintf(buffer, size, "%.2f %s", value, units[unit_idx]);
    }
}

/* String utilities */
char* trim_whitespace(char *str) {
    char *end;
    
    /* Trim leading space */
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    if (*str == 0) {
        return str;
    }
    
    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    end[1] = '\0';
    
    return str;
}

/* Split string by delimiter */
int split_string(char *str, char delimiter, char **tokens, int max_tokens) {
    int count = 0;
    char *token = str;
    char *next;
    
    while (count < max_tokens && token) {
        next = strchr(token, delimiter);
        if (next) {
            *next = '\0';
            next++;
        }
        
        tokens[count++] = trim_whitespace(token);
        token = next;
    }
    
    return count;
}

/* Check if file exists */
int file_exists(const char *path) {
    return access(path, F_OK) == 0;
}

/* Read file contents */
char* read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);
    
    return buffer;
}

/* Write file contents */
int write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) {
        return CR_ESYSCALL;
    }
    
    fputs(content, f);
    fclose(f);
    return CR_OK;
}
