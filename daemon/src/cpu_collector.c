#define _POSIX_C_SOURCE 200809L

#include "sentinel/cpu_collector.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROC_STAT_DEFAULT "/proc/stat"

static bool read_counter(const char **cursor, uint64_t *counter)
{
    char *end;
    unsigned long long value;

    while (isspace((unsigned char)**cursor)) {
        ++*cursor;
    }
    if (**cursor == '\0' || **cursor == '-' || **cursor == '+') {
        return false;
    }

    errno = 0;
    value = strtoull(*cursor, &end, 10);
    if (errno == ERANGE || end == *cursor ||
        (*end != '\0' && !isspace((unsigned char)*end))) {
        return false;
    }

    *counter = (uint64_t)value;
    *cursor = end;
    return true;
}

bool cpu_collector_parse(const char *line, struct cpu_times *times)
{
    uint64_t counters[8] = {0};
    size_t count = 0;
    const char *cursor;

    if (line == NULL || times == NULL || strlen(line) < 4 ||
        strncmp(line, "cpu", 3) != 0 ||
        !isspace((unsigned char)line[3])) {
        return false;
    }

    cursor = line + 3;
    while (true) {
        const char *before = cursor;
        uint64_t ignored;

        while (isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        if (!read_counter(&cursor, count < 8 ? &counters[count] : &ignored)) {
            return false;
        }
        if (count < 8) {
            ++count;
        }
        if (cursor == before) {
            return false;
        }
    }

    if (count < 4) {
        return false;
    }

    *times = (struct cpu_times) {
        .user = counters[0],
        .nice = counters[1],
        .system = counters[2],
        .idle = counters[3],
        .iowait = counters[4],
        .irq = counters[5],
        .softirq = counters[6],
        .steal = counters[7],
    };
    return true;
}

static bool total_ticks(const struct cpu_times *times, uint64_t *total)
{
    const uint64_t counters[] = {
        times->user, times->nice, times->system, times->idle,
        times->iowait, times->irq, times->softirq, times->steal,
    };
    uint64_t sum = 0;

    for (size_t i = 0; i < sizeof(counters) / sizeof(counters[0]); ++i) {
        if (UINT64_MAX - sum < counters[i]) {
            return false;
        }
        sum += counters[i];
    }

    *total = sum;
    return true;
}

static enum collector_result cpu_collect(void *context,
                                        struct telemetry_sample *sample,
                                        char *error,
                                        size_t error_size)
{
    struct cpu_collector_state *state = context;
    struct cpu_times current;
    char *line = NULL;
    size_t line_capacity = 0;
    FILE *file;
    uint64_t previous_total;
    uint64_t current_total;
    uint64_t total_delta;
    uint64_t idle_delta;
    uint64_t previous_idle;
    uint64_t current_idle;
    int64_t timestamp_ms;

    file = fopen(state->proc_stat_path, "r");
    if (file == NULL) {
        (void)snprintf(error, error_size, "cannot open %s: %s",
                       state->proc_stat_path, strerror(errno));
        return COLLECTOR_FAILURE;
    }

    if (getline(&line, &line_capacity, file) == -1) {
        (void)snprintf(error, error_size, "cannot read %s", state->proc_stat_path);
        free(line);
        (void)fclose(file);
        return COLLECTOR_FAILURE;
    }
    (void)fclose(file);

    if (!cpu_collector_parse(line, &current)) {
        (void)snprintf(error, error_size, "invalid aggregate CPU line in %s",
                       state->proc_stat_path);
        free(line);
        return COLLECTOR_FAILURE;
    }
    free(line);

    if (!state->has_previous) {
        state->previous = current;
        state->has_previous = true;
        return COLLECTOR_NO_DATA;
    }

    if (!total_ticks(&state->previous, &previous_total) ||
        !total_ticks(&current, &current_total) ||
        current_total < previous_total ||
        current.idle < state->previous.idle ||
        current.iowait < state->previous.iowait) {
        state->previous = current;
        (void)snprintf(error, error_size, "CPU counters decreased or overflowed");
        return COLLECTOR_FAILURE;
    }

    total_delta = current_total - previous_total;
    if (UINT64_MAX - state->previous.idle < state->previous.iowait ||
        UINT64_MAX - current.idle < current.iowait) {
        state->previous = current;
        (void)snprintf(error, error_size, "CPU idle counters overflowed");
        return COLLECTOR_FAILURE;
    }
    previous_idle = state->previous.idle + state->previous.iowait;
    current_idle = current.idle + current.iowait;
    if (current_idle < previous_idle) {
        state->previous = current;
        (void)snprintf(error, error_size, "CPU idle counters decreased or overflowed");
        return COLLECTOR_FAILURE;
    }
    idle_delta = current_idle - previous_idle;
    state->previous = current;

    if (total_delta == 0 || idle_delta > total_delta) {
        return COLLECTOR_NO_DATA;
    }
    if (telemetry_collector_timestamp_ms(&timestamp_ms) == -1) {
        (void)snprintf(error, error_size, "cannot read current timestamp");
        return COLLECTOR_FAILURE;
    }

    if (!telemetry_sample_init(sample, "cpu.utilization",
                               100.0 * (double)(total_delta - idle_delta) /
                                   (double)total_delta,
                               "%", timestamp_ms, "cpu")) {
        (void)snprintf(error, error_size, "cannot create CPU telemetry sample");
        return COLLECTOR_FAILURE;
    }

    return COLLECTOR_SUCCESS;
}

void cpu_collector_init(struct telemetry_collector *collector,
                        struct cpu_collector_state *state,
                        const char *proc_stat_path)
{
    state->previous = (struct cpu_times){0};
    state->has_previous = false;
    state->proc_stat_path =
        proc_stat_path != NULL ? proc_stat_path : PROC_STAT_DEFAULT;
    collector->name = "cpu";
    collector->context = state;
    collector->collect = cpu_collect;
}
