#ifndef SENTINEL_MEMORY_COLLECTOR_H
#define SENTINEL_MEMORY_COLLECTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "sentinel/collector.h"

struct memory_info {
    uint64_t total_kib;
    uint64_t available_kib;
};

struct memory_collector_state {
    const char *proc_meminfo_path;
};

/* Parse MemTotal and MemAvailable from /proc/meminfo. */
bool memory_collector_parse(const char *text, struct memory_info *info);

/* proc_meminfo_path is borrowed; NULL selects /proc/meminfo. */
void memory_collector_init(struct telemetry_collector *collector,
                           struct memory_collector_state *state,
                           const char *proc_meminfo_path);

#endif
