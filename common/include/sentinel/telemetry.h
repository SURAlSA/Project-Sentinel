#ifndef SENTINEL_TELEMETRY_H
#define SENTINEL_TELEMETRY_H

#include <stdbool.h>
#include <stdint.h>

struct telemetry_sample {
    const char *metric;
    double value;
    const char *unit;
    int64_t timestamp_ms;
    const char *source;
};

/*
 * Initialize a sample using borrowed strings. The strings must remain valid
 * while the sample is used. On invalid input, returns false and leaves sample
 * unchanged. A NULL source is allowed.
 */
bool telemetry_sample_init(struct telemetry_sample *sample,
                           const char *metric,
                           double value,
                           const char *unit,
                           int64_t timestamp_ms,
                           const char *source);

/* A valid sample has non-empty metric and unit, a finite value, and a
 * non-negative Unix-epoch timestamp in milliseconds. Source is optional. */
bool telemetry_sample_is_valid(const struct telemetry_sample *sample);

#endif