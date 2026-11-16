#include "sentinel/scheduler.h"

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

static void test_fixed_interval_deadlines(void)
{
    const struct timespec start = {.tv_sec = 10, .tv_nsec = 250000000};
    const struct timespec after_collection = {
        .tv_sec = 10,
        .tv_nsec = 800000000,
    };
    const struct timespec late_collection = {
        .tv_sec = 15,
        .tv_nsec = 800000000,
    };
    const struct timespec at_deadline = {
        .tv_sec = 16,
        .tv_nsec = 250000000,
    };
    struct sampling_scheduler scheduler;
    struct timespec deadline;

    CHECK(sampling_scheduler_init(&scheduler, 1000, &start) == 0);
    CHECK(sampling_scheduler_next_deadline(&scheduler, &after_collection,
                                           &deadline) == 0);
    CHECK(deadline.tv_sec == 11);
    CHECK(deadline.tv_nsec == 250000000);

    /* Slow collection skips missed intervals instead of causing catch-up runs. */
    CHECK(sampling_scheduler_next_deadline(&scheduler, &late_collection,
                                           &deadline) == 0);
    CHECK(deadline.tv_sec == 16);
    CHECK(deadline.tv_nsec == 250000000);
    CHECK(sampling_scheduler_next_deadline(&scheduler, &at_deadline,
                                           &deadline) == 0);
    CHECK(deadline.tv_sec == 17);
    CHECK(deadline.tv_nsec == 250000000);
}

static void test_invalid_scheduler_inputs(void)
{
    const struct timespec valid_time = {.tv_sec = 1, .tv_nsec = 0};
    const struct timespec invalid_time = {.tv_sec = 1, .tv_nsec = 1000000000};
    struct sampling_scheduler scheduler;
    struct timespec deadline;

    CHECK(sampling_scheduler_init(&scheduler, 99, &valid_time) == -1);
    CHECK(sampling_scheduler_init(&scheduler, 60001, &valid_time) == -1);
    CHECK(sampling_scheduler_init(&scheduler, 1000, &invalid_time) == -1);
    CHECK(sampling_scheduler_init(NULL, 1000, &valid_time) == -1);
    CHECK(sampling_scheduler_init(&scheduler, 1000, &valid_time) == 0);
    CHECK(sampling_scheduler_next_deadline(&scheduler, NULL, &deadline) == -1);
    CHECK(sampling_scheduler_next_deadline(&scheduler, &valid_time, NULL) == -1);
}

int main(void)
{
    test_fixed_interval_deadlines();
    test_invalid_scheduler_inputs();

    if (failures != 0) {
        fprintf(stderr, "%d scheduler test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
