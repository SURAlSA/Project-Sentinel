#define _POSIX_C_SOURCE 200809L

#include "sentinel/collector.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void set_error(char *error, size_t error_size, const char *message)
{
    if (error != NULL && error_size > 0) {
        (void)snprintf(error, error_size, "%s", message);
    }
}

enum collector_result telemetry_collector_collect(
    const struct telemetry_collector *collector,
    struct telemetry_sample *sample,
    char *error,
    size_t error_size)
{
    enum collector_result result;

    if (error != NULL && error_size > 0) {
        error[0] = '\0';
    }

    if (collector == NULL || collector->name == NULL ||
        collector->collect == NULL || sample == NULL) {
        set_error(error, error_size, "invalid collector or sample argument");
        return COLLECTOR_FAILURE;
    }

    if (error == NULL) {
        error_size = 0;
    }
    result = collector->collect(collector->context, sample, error, error_size);
    if (result == COLLECTOR_SUCCESS &&
        !telemetry_sample_is_valid(sample)) {
        set_error(error, error_size, "collector produced an invalid sample");
        return COLLECTOR_FAILURE;
    }
    if (result == COLLECTOR_FAILURE &&
        (error == NULL || error_size == 0 || error[0] == '\0')) {
        if (error != NULL && error_size > 0) {
            (void)snprintf(error, error_size, "collector '%s' failed",
                           collector->name);
        }
    }
    if (result != COLLECTOR_SUCCESS && result != COLLECTOR_NO_DATA &&
        result != COLLECTOR_FAILURE) {
        set_error(error, error_size, "collector returned an invalid result");
        return COLLECTOR_FAILURE;
    }

    return result;
}

int telemetry_collector_timestamp_ms(int64_t *timestamp_ms)
{
    struct timespec now;
    int64_t seconds_ms;

    if (timestamp_ms == NULL || clock_gettime(CLOCK_REALTIME, &now) == -1 ||
        now.tv_sec < 0 || now.tv_sec > INT64_MAX / 1000) {
        return -1;
    }

    seconds_ms = (int64_t)now.tv_sec * 1000;
    *timestamp_ms = seconds_ms + now.tv_nsec / 1000000;
    return 0;
}
