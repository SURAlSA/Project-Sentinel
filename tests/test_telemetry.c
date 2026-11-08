#include "sentinel/telemetry.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

/*
 * Evaluate a test condition and keep going after a failure, so one test run
 * reports all failed checks instead of stopping at the first one.
 */
#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,  \
                    #condition);                                               \
            ++failures;                                                        \
        }                                                                       \
    } while (0)

static void test_valid_sample(void)
{
    struct telemetry_sample sample;

    CHECK(telemetry_sample_init(&sample, "cpu.utilization", 42.5, "%", 1234,
                                "cpu0"));
    CHECK(telemetry_sample_is_valid(&sample));
    CHECK(strcmp(sample.metric, "cpu.utilization") == 0);
    CHECK(sample.value == 42.5);
    CHECK(strcmp(sample.unit, "%") == 0);
    CHECK(sample.timestamp_ms == 1234);
    CHECK(strcmp(sample.source, "cpu0") == 0);
}

static void test_optional_source(void)
{
    struct telemetry_sample sample;

    /* NULL is valid for source because identifying a particular sensor is optional. */
    CHECK(telemetry_sample_init(&sample, "memory.used", 1024.0, "bytes", 0,
                                NULL));
    CHECK(sample.source == NULL);
    CHECK(telemetry_sample_is_valid(&sample));
}

static void test_invalid_samples(void)
{
    struct telemetry_sample sample = {
        .metric = "unchanged",
        .value = 1.0,
        .unit = "count",
        .timestamp_ms = 9,
        .source = NULL,
    };
    /* Keep a copy to verify that each rejected initialization preserves the value. */
    const struct telemetry_sample original = sample;

    /* Cover null/empty strings, non-finite numbers, and an invalid timestamp. */
    CHECK(!telemetry_sample_init(NULL, "metric", 1.0, "unit", 0, NULL));
    CHECK(!telemetry_sample_init(&sample, NULL, 1.0, "unit", 0, NULL));
    CHECK(!telemetry_sample_init(&sample, "", 1.0, "unit", 0, NULL));
    CHECK(!telemetry_sample_init(&sample, "metric", NAN, "unit", 0, NULL));
    CHECK(!telemetry_sample_init(&sample, "metric", INFINITY, "unit", 0,
                                 NULL));
    CHECK(!telemetry_sample_init(&sample, "metric", 1.0, NULL, 0, NULL));
    CHECK(!telemetry_sample_init(&sample, "metric", 1.0, "", 0, NULL));
    CHECK(!telemetry_sample_init(&sample, "metric", 1.0, "unit", -1, NULL));
    CHECK(!telemetry_sample_init(&sample, "metric", 1.0, "unit", 0, ""));
    CHECK(sample.metric == original.metric);
    CHECK(sample.value == original.value);
    CHECK(sample.unit == original.unit);
    CHECK(sample.timestamp_ms == original.timestamp_ms);
    CHECK(sample.source == original.source);

    CHECK(!telemetry_sample_is_valid(NULL));
}

int main(void)
{
    test_valid_sample();
    test_optional_source();
    test_invalid_samples();

    if (failures != 0) {
        fprintf(stderr, "%d telemetry test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
