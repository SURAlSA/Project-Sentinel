#define _POSIX_C_SOURCE 200809L

#include "sentinel/memory_collector.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROC_MEMINFO_DEFAULT "/proc/meminfo"

static bool parse_kib_value(const char *text, size_t length, uint64_t *value)
{
    size_t position = 0;
    uint64_t parsed = 0;
    size_t digits = 0;

    while (position < length && isspace((unsigned char)text[position])) {
        ++position;
    }
    while (position < length && text[position] >= '0' &&
           text[position] <= '9') {
        const unsigned int digit = (unsigned int)(text[position] - '0');

        if (parsed > (UINT64_MAX - digit) / 10) {
            return false;
        }
        parsed = parsed * 10 + digit;
        ++position;
        ++digits;
    }
    if (digits == 0) {
        return false;
    }

    while (position < length && isspace((unsigned char)text[position])) {
        ++position;
    }
    if (length - position < 2 || text[position] != 'k' ||
        text[position + 1] != 'B') {
        return false;
    }
    position += 2;
    while (position < length && isspace((unsigned char)text[position])) {
        ++position;
    }
    if (position != length) {
        return false;
    }

    *value = parsed;
    return true;
}

bool memory_collector_parse(const char *text, struct memory_info *info)
{
    bool found_total = false;
    bool found_available = false;
    struct memory_info parsed = {0};
    const char *line = text;

    if (text == NULL || info == NULL) {
        return false;
    }

    while (*line != '\0') {
        const char *line_end = strchr(line, '\n');
        const char *colon = strchr(line, ':');
        const size_t line_length = line_end != NULL
                                       ? (size_t)(line_end - line)
                                       : strlen(line);

        if (colon != NULL && (size_t)(colon - line) < line_length) {
            const size_t key_length = (size_t)(colon - line);
            uint64_t value;
            bool is_total = key_length == sizeof("MemTotal") - 1 &&
                            strncmp(line, "MemTotal", key_length) == 0;
            bool is_available = key_length == sizeof("MemAvailable") - 1 &&
                                strncmp(line, "MemAvailable", key_length) == 0;

            if (is_total || is_available) {
                const char *value_text = colon + 1;
                const size_t value_length =
                    line_length - (size_t)(value_text - line);

                if (!parse_kib_value(value_text, value_length, &value)) {
                    return false;
                }
                if (is_total) {
                    if (found_total) {
                        return false;
                    }
                    parsed.total_kib = value;
                    found_total = true;
                } else {
                    if (found_available) {
                        return false;
                    }
                    parsed.available_kib = value;
                    found_available = true;
                }
            }
        }

        if (line_end == NULL) {
            break;
        }
        line = line_end + 1;
    }

    if (!found_total || !found_available || parsed.total_kib == 0 ||
        parsed.available_kib > parsed.total_kib) {
        return false;
    }

    *info = parsed;
    return true;
}

static bool append_text(char **buffer,
                        size_t *length,
                        size_t *capacity,
                        const char *line,
                        size_t line_length)
{
    char *expanded;

    if (line_length > SIZE_MAX - *length - 1) {
        return false;
    }
    if (*length + line_length + 1 > *capacity) {
        size_t new_capacity = *capacity == 0 ? 1024 : *capacity;

        while (new_capacity < *length + line_length + 1) {
            if (new_capacity > SIZE_MAX / 2) {
                new_capacity = *length + line_length + 1;
                break;
            }
            new_capacity *= 2;
        }
        expanded = realloc(*buffer, new_capacity);
        if (expanded == NULL) {
            return false;
        }
        *buffer = expanded;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *length, line, line_length);
    *length += line_length;
    (*buffer)[*length] = '\0';
    return true;
}

static enum collector_result memory_collect(void *context,
                                           struct telemetry_sample *sample,
                                           char *error,
                                           size_t error_size)
{
    const struct memory_collector_state *state = context;
    char *line = NULL;
    size_t line_capacity = 0;
    char *text = NULL;
    size_t text_length = 0;
    size_t text_capacity = 0;
    FILE *file = fopen(state->proc_meminfo_path, "r");
    struct memory_info info;
    int64_t timestamp_ms;

    if (file == NULL) {
        (void)snprintf(error, error_size, "cannot open %s: %s",
                       state->proc_meminfo_path, strerror(errno));
        return COLLECTOR_FAILURE;
    }
    while (true) {
        const ssize_t line_length = getline(&line, &line_capacity, file);

        if (line_length == -1) {
            break;
        }
        if (!append_text(&text, &text_length, &text_capacity, line,
                         (size_t)line_length)) {
            free(line);
            free(text);
            (void)fclose(file);
            (void)snprintf(error, error_size, "cannot buffer %s",
                           state->proc_meminfo_path);
            return COLLECTOR_FAILURE;
        }
    }
    free(line);
    if (ferror(file)) {
        free(text);
        (void)fclose(file);
        (void)snprintf(error, error_size, "error reading %s",
                       state->proc_meminfo_path);
        return COLLECTOR_FAILURE;
    }
    (void)fclose(file);

    if (text == NULL || !memory_collector_parse(text, &info)) {
        free(text);
        (void)snprintf(error, error_size, "invalid memory data in %s",
                       state->proc_meminfo_path);
        return COLLECTOR_FAILURE;
    }
    free(text);

    if (telemetry_collector_timestamp_ms(&timestamp_ms) == -1) {
        (void)snprintf(error, error_size, "cannot read current timestamp");
        return COLLECTOR_FAILURE;
    }

    if (!telemetry_sample_init(
            sample, "memory.utilization",
            100.0 * (double)(info.total_kib - info.available_kib) /
                (double)info.total_kib,
            "%", timestamp_ms, NULL)) {
        (void)snprintf(error, error_size, "cannot create memory telemetry sample");
        return COLLECTOR_FAILURE;
    }

    return COLLECTOR_SUCCESS;
}

void memory_collector_init(struct telemetry_collector *collector,
                           struct memory_collector_state *state,
                           const char *proc_meminfo_path)
{
    state->proc_meminfo_path = proc_meminfo_path != NULL
                                   ? proc_meminfo_path
                                   : PROC_MEMINFO_DEFAULT;
    collector->name = "memory";
    collector->context = state;
    collector->collect = memory_collect;
}
