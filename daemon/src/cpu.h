#ifndef SENTINEL_CPU_H
#define SENTINEL_CPU_H

#include <stdbool.h>

struct cpu_times {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
};

bool cpu_read_times(struct cpu_times *times);


#endif