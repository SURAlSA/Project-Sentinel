#include "sentinel/collector.h"
#include "sentinel/cpu_collector.h"
#include "sentinel/memory_collector.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,  \
                    #condition);                                               \
            ++failures;                                                        \
        }                                                                       \
    } while (0)

struct mock_context {
    int calls;
    bool should_fail;
};

static enum collector_result mock_collect(void *context,
                                          struct telemetry_sample *sample,
                                          char *error,
                                          size_t error_size)
{
    struct mock_context *mock = context;

    ++mock->calls;
    if (mock->should_fail) {
        (void)snprintf(error, error_size, "test failure");
        return COLLECTOR_FAILURE;
    }

    if (!telemetry_sample_init(sample, "test.metric", 7.0, "units", 100, NULL)) {
        (void)snprintf(error, error_size, "test sample initialization failed");
        return COLLECTOR_FAILURE;
    }
    return COLLECTOR_SUCCESS;
}

static void test_collector_invocation_and_errors(void)
{
    struct mock_context context = {0};
    struct telemetry_collector collector = {
        .name = "mock",
        .context = &context,
        .collect = mock_collect,
    };
    struct telemetry_sample sample;
    char error[64];

    CHECK(telemetry_collector_collect(&collector, &sample, error,
                                      sizeof(error)) == COLLECTOR_SUCCESS);
    CHECK(context.calls == 1);
    CHECK(strcmp(sample.metric, "test.metric") == 0);
    CHECK(error[0] == '\0');

    context.should_fail = true;
    CHECK(telemetry_collector_collect(&collector, &sample, error,
                                      sizeof(error)) == COLLECTOR_FAILURE);
    CHECK(context.calls == 2);
    CHECK(strcmp(error, "test failure") == 0);
}

static void test_cpu_parser(void)
{
    const char *line = "cpu 100 20 30 400 5 6 7 8 9 10\n";
    struct cpu_times times = {0};

    CHECK(cpu_collector_parse(line, &times));
    CHECK(times.user == 100);
    CHECK(times.nice == 20);
    CHECK(times.system == 30);
    CHECK(times.idle == 400);
    CHECK(times.iowait == 5);
    CHECK(times.irq == 6);
    CHECK(times.softirq == 7);
    CHECK(times.steal == 8);

    CHECK(cpu_collector_parse("cpu 1 2 3 4\n", &times));
    CHECK(times.user == 1);
    CHECK(times.iowait == 0);
    CHECK(!cpu_collector_parse("cpu0 1 2 3 4\n", &times));
    CHECK(!cpu_collector_parse("cpu 1 2 3\n", &times));
    CHECK(!cpu_collector_parse("cpu 1 2 invalid 4\n", &times));
    CHECK(!cpu_collector_parse("cpu -1 2 3 4\n", &times));
    CHECK(!cpu_collector_parse("cpu", &times));
}

static void test_memory_parser(void)
{
    const char *text =
        "MemTotal:       1000 kB\n"
        "MemFree:         300 kB\n"
        "MemAvailable:    600 kB\n"
        "Buffers:          20 kB\n";
    struct memory_info info = {0};

    CHECK(memory_collector_parse(text, &info));
    CHECK(info.total_kib == 1000);
    CHECK(info.available_kib == 600);

    CHECK(!memory_collector_parse("MemAvailable: 600 kB\n", &info));
    CHECK(!memory_collector_parse(
        "MemTotal: 1000 kB\nMemAvailable: 1001 kB\n", &info));
    CHECK(!memory_collector_parse(
        "MemTotal: 1000 MB\nMemAvailable: 500 kB\n", &info));
    CHECK(!memory_collector_parse(
        "MemTotal: 1000 kB\nMemTotal: 1000 kB\n"
        "MemAvailable: 500 kB\n", &info));
}

int main(void)
{
    test_collector_invocation_and_errors();
    test_cpu_parser();
    test_memory_parser();

    if (failures != 0) {
        fprintf(stderr, "%d collector test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
