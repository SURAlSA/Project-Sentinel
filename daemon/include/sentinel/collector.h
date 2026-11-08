#ifndef SENTINEL_COLLECTOR_H
#define SENTINEL_COLLECTOR_H

#include <stddef.h>
#include <stdint.h>

#include "sentinel/telemetry.h"

enum collector_result {
    COLLECTOR_SUCCESS,
    COLLECTOR_NO_DATA,
    COLLECTOR_FAILURE
};

struct telemetry_collector {
    const char *name;
    void *context;
    enum collector_result (*collect)(void *context,
                                     struct telemetry_sample *sample,
                                     char *error,
                                     size_t error_size);
};

/*
 * Run one collector. Errors are written to error when a buffer is supplied.
 * The sample is valid only when COLLECTOR_SUCCESS is returned. The callback
 * context is owned by the caller and must outlive the collector.
 */
enum collector_result telemetry_collector_collect(
    const struct telemetry_collector *collector,
    struct telemetry_sample *sample,
    char *error,
    size_t error_size);

/* Read the current Unix-epoch time in milliseconds. */
int telemetry_collector_timestamp_ms(int64_t *timestamp_ms);

#endif
