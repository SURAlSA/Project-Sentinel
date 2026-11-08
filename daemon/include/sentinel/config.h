#ifndef SENTINEL_CONFIG_H
#define SENTINEL_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#define SENTINEL_CONFIG_PATH_MAX 4096
#define SENTINEL_MIN_SAMPLING_INTERVAL_MS 100
#define SENTINEL_MAX_SAMPLING_INTERVAL_MS 60000

enum sentinel_log_level {
    SENTINEL_LOG_DEBUG,
    SENTINEL_LOG_INFO,
    SENTINEL_LOG_WARN,
    SENTINEL_LOG_ERROR
};

struct sentinel_config {
    unsigned int sampling_interval_ms;
    char database_path[SENTINEL_CONFIG_PATH_MAX];
    enum sentinel_log_level log_level;
    bool enable_remote_telemetry;
};

/* Populate config with the documented safe defaults. */
void sentinel_config_defaults(struct sentinel_config *config);

/*
 * Load the supported TOML-style settings from path. Missing settings keep
 * their defaults. Returns 0 on success and -1 with a diagnostic on failure.
 */
int sentinel_config_load(const char *path,
                         struct sentinel_config *config,
                         char *error,
                         size_t error_size);

const char *sentinel_log_level_name(enum sentinel_log_level level);

#endif
