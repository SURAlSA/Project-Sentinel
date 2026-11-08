#ifndef SENTINEL_CPU_COLLECTOR_H
#define SENTINEL_CPU_COLLECTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "sentinel/collector.h"

struct cpu_times {
    uint64_t user;
    uint64_t nice;
    uint64_t system;
    uint64_t idle;
    uint64_t iowait;
    uint64_t irq;
    uint64_t softirq;
    uint64_t steal;
};

struct cpu_collector_state {
    struct cpu_times previous;
    bool has_previous;
    const char *proc_stat_path;
};

/* Parse the aggregate "cpu" line from /proc/stat. */
bool cpu_collector_parse(const char *line, struct cpu_times *times);

/* proc_stat_path is borrowed; NULL selects /proc/stat. */
void cpu_collector_init(struct telemetry_collector *collector,
                        struct cpu_collector_state *state,
                        const char *proc_stat_path);

#endif
