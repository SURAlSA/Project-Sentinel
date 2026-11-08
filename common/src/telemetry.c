#include "sentinel/telemetry.h"

#include <math.h>
#include <stddef.h>

static bool is_non_empty(const char *text)
{
    /* Check for NULL before indexing: C strings are pointers, not string objects. */
    return text != NULL && text[0] != '\0';
}

bool telemetry_sample_is_valid(const struct telemetry_sample *sample)
{
    /* A NULL source means "not specified"; an empty source is still invalid. */
    return sample != NULL &&
           is_non_empty(sample->metric) &&
           is_non_empty(sample->unit) &&
           isfinite(sample->value) &&
           sample->timestamp_ms >= 0 &&
           (sample->source == NULL || is_non_empty(sample->source));
}

bool telemetry_sample_init(struct telemetry_sample *sample,
                           const char *metric,
                           double value,
                           const char *unit,
                           int64_t timestamp_ms,
                           const char *source)
{
    /*
     * Build and validate a temporary value first, so a failed init never leaves
     * the caller's existing sample partially overwritten.
     */
    const struct telemetry_sample candidate = {
        .metric = metric,
        .value = value,
        .unit = unit,
        .timestamp_ms = timestamp_ms,
        .source = source,
    };

    if (sample == NULL || !telemetry_sample_is_valid(&candidate)) {
        return false;
    }

    /* The struct assignment copies the pointers, not the strings they point to. */
    *sample = candidate;
    return true;
}
