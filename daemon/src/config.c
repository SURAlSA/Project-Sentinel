// tells the compiler that the program wants access to POSIX.1-2008 functionality.
// POSIX is a standard that defines interfaces for Unix-like operating systems, including Linux.
#define _POSIX_C_SOURCE 200809L

#include "sentinel/config.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_DATABASE_PATH "/var/lib/sentinel/data.db"

enum config_key {
    CONFIG_KEY_SAMPLING_INTERVAL,
    CONFIG_KEY_DATABASE_PATH,
    CONFIG_KEY_LOG_LEVEL,
    CONFIG_KEY_REMOTE_TELEMETRY,
    CONFIG_KEY_COUNT
};

static void set_error(char *error,
                      size_t error_size,
                      const char *path,
                      size_t line_number,
                      const char *message)
{
    if (error == NULL || error_size == 0) {
        return;
    }

    if (line_number == 0) {
        (void)snprintf(error, error_size, "%s: %s", path, message);
    } else {
        (void)snprintf(error, error_size, "%s:%zu: %s", path, line_number,
                       message);
    }
}

static char *trim(char *text)
{
    char *end;

    while (isspace((unsigned char)*text)) {
        ++text;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        --end;
    }
    *end = '\0';
    return text;
}

static char *find_comment(char *text)
{
    bool in_string = false;
    bool escaped = false;

    for (char *cursor = text; *cursor != '\0'; ++cursor) {
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (*cursor == '\\') {
                escaped = true;
            } else if (*cursor == '"') {
                in_string = false;
            }
        } else if (*cursor == '"') {
            in_string = true;
        } else if (*cursor == '#') {
            return cursor;
        }
    }
    return NULL;
}

static bool decode_string(const char *value,
                          char *destination,
                          size_t destination_size)
{
    size_t output_length = 0;
    const size_t input_length = strlen(value);

    if (input_length < 2 || value[0] != '"' ||
        value[input_length - 1] != '"') {
        return false;
    }

    for (size_t i = 1; i < input_length - 1; ++i) {
        unsigned char character = (unsigned char)value[i];

        if (character == '\\') {
            ++i;
            if (i >= input_length - 1) {
                return false;
            }
            switch (value[i]) {
            case '"':
            case '\\':
                character = (unsigned char)value[i];
                break;
            case 't':
                character = '\t';
                break;
            default:
                return false;
            }
        } else if (character == '"') {
            return false;
        }

        if (character < 0x20 || character == 0x7f ||
            output_length + 1 >= destination_size) {
            return false;
        }
        destination[output_length++] = (char)character;
    }

    if (output_length == 0) {
        return false;
    }
    destination[output_length] = '\0';
    return true;
}

static bool parse_interval(const char *value, unsigned int *interval_ms)
{
    char *end;
    unsigned long parsed;

    if (*value == '\0' || *value == '-' || *value == '+') {
        return false;
    }
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno == ERANGE || end == value || *end != '\0' ||
        parsed < SENTINEL_MIN_SAMPLING_INTERVAL_MS ||
        parsed > SENTINEL_MAX_SAMPLING_INTERVAL_MS) {
        return false;
    }

    *interval_ms = (unsigned int)parsed;
    return true;
}

static bool parse_log_level(const char *value, enum sentinel_log_level *level)
{
    if (strcmp(value, "\"debug\"") == 0) {
        *level = SENTINEL_LOG_DEBUG;
    } else if (strcmp(value, "\"info\"") == 0) {
        *level = SENTINEL_LOG_INFO;
    } else if (strcmp(value, "\"warn\"") == 0) {
        *level = SENTINEL_LOG_WARN;
    } else if (strcmp(value, "\"error\"") == 0) {
        *level = SENTINEL_LOG_ERROR;
    } else {
        return false;
    }
    return true;
}

static bool parse_boolean(const char *value, bool *enabled)
{
    if (strcmp(value, "true") == 0) {
        *enabled = true;
    } else if (strcmp(value, "false") == 0) {
        *enabled = false;
    } else {
        return false;
    }
    return true;
}

static enum config_key identify_key(const char *key)
{
    if (strcmp(key, "sampling_interval_ms") == 0) {
        return CONFIG_KEY_SAMPLING_INTERVAL;
    }
    if (strcmp(key, "database_path") == 0) {
        return CONFIG_KEY_DATABASE_PATH;
    }
    if (strcmp(key, "log_level") == 0) {
        return CONFIG_KEY_LOG_LEVEL;
    }
    if (strcmp(key, "enable_remote_telemetry") == 0) {
        return CONFIG_KEY_REMOTE_TELEMETRY;
    }
    return CONFIG_KEY_COUNT;
}

void sentinel_config_defaults(struct sentinel_config *config)
{
    if (config == NULL) {
        return;
    }

    config->sampling_interval_ms = 1000;
    (void)snprintf(config->database_path, sizeof(config->database_path), "%s",
                   DEFAULT_DATABASE_PATH);
    config->log_level = SENTINEL_LOG_INFO;
    config->enable_remote_telemetry = false;
}

int sentinel_config_load(const char *path,
                         struct sentinel_config *config,
                         char *error,
                         size_t error_size)
{
    bool seen[CONFIG_KEY_COUNT] = {false};
    char *line = NULL;
    size_t capacity = 0;
    size_t line_number = 0;
    struct stat file_info;
    FILE *file;
    int descriptor;

    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }
    if (path == NULL || path[0] == '\0' || config == NULL) {
        set_error(error, error_size, path != NULL ? path : "<config>", 0,
                  "configuration path and destination are required");
        return -1;
    }

    sentinel_config_defaults(config);
    descriptor = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (descriptor == -1) {
        char message[256];
        (void)snprintf(message, sizeof(message),
                       "cannot open configuration: %s (check the path and permissions)",
                       strerror(errno));
        set_error(error, error_size, path, 0, message);
        return -1;
    }
    if (fstat(descriptor, &file_info) == -1 || !S_ISREG(file_info.st_mode)) {
        set_error(error, error_size, path, 0,
                  "configuration must be a regular file");
        (void)close(descriptor);
        return -1;
    }
    file = fdopen(descriptor, "r");
    if (file == NULL) {
        char message[256];
        (void)snprintf(message, sizeof(message),
                       "cannot read configuration: %s", strerror(errno));
        set_error(error, error_size, path, 0, message);
        (void)close(descriptor);
        return -1;
    }

    while (getline(&line, &capacity, file) != -1) {
        char *comment;
        char *equals;
        char *key;
        char *value;
        enum config_key key_id;
        bool valid = false;

        ++line_number;
        comment = find_comment(line);
        if (comment != NULL) {
            *comment = '\0';
        }
        key = trim(line);
        if (*key == '\0') {
            continue;
        }
        equals = strchr(key, '=');
        if (equals == NULL) {
            set_error(error, error_size, path, line_number,
                      "expected key = value");
            goto failure;
        }
        *equals = '\0';
        key = trim(key);
        value = trim(equals + 1);
        if (*key == '\0' || *value == '\0') {
            set_error(error, error_size, path, line_number,
                      "expected one key and one value");
            goto failure;
        }

        key_id = identify_key(key);
        if (key_id == CONFIG_KEY_COUNT) {
            set_error(error, error_size, path, line_number,
                      "unknown setting");
            goto failure;
        }
        if (seen[key_id]) {
            set_error(error, error_size, path, line_number,
                      "duplicate setting");
            goto failure;
        }

        switch (key_id) {
        case CONFIG_KEY_SAMPLING_INTERVAL:
            valid = parse_interval(value, &config->sampling_interval_ms);
            if (!valid) {
                set_error(error, error_size, path, line_number,
                          "sampling_interval_ms must be an integer from 100 to 60000");
            }
            break;
        case CONFIG_KEY_DATABASE_PATH:
            valid = decode_string(value, config->database_path,
                                  sizeof(config->database_path));
            if (!valid) {
                set_error(error, error_size, path, line_number,
                          "database_path must be a non-empty quoted path (maximum 4095 bytes)");
            }
            break;
        case CONFIG_KEY_LOG_LEVEL:
            valid = parse_log_level(value, &config->log_level);
            if (!valid) {
                set_error(error, error_size, path, line_number,
                          "log_level must be \"debug\", \"info\", \"warn\", or \"error\"");
            }
            break;
        case CONFIG_KEY_REMOTE_TELEMETRY:
            valid = parse_boolean(value, &config->enable_remote_telemetry);
            if (!valid) {
                set_error(error, error_size, path, line_number,
                          "enable_remote_telemetry must be true or false");
            }
            break;
        case CONFIG_KEY_COUNT:
            break;
        }
        if (!valid) {
            goto failure;
        }
        seen[key_id] = true;
    }

    if (ferror(file)) {
        set_error(error, error_size, path, line_number,
                  "error reading configuration");
        goto failure;
    }

    free(line);
    (void)fclose(file);
    return 0;

failure:
    free(line);
    (void)fclose(file);
    return -1;
}

const char *sentinel_log_level_name(enum sentinel_log_level level)
{
    switch (level) {
    case SENTINEL_LOG_DEBUG:
        return "debug";
    case SENTINEL_LOG_INFO:
        return "info";
    case SENTINEL_LOG_WARN:
        return "warn";
    case SENTINEL_LOG_ERROR:
        return "error";
    }
    return "invalid";
}
