#include "sentinel/scheduler.h"

#include "sentinel/config.h"

#include <stdbool.h>
#include <stdint.h>

#define NANOSECONDS_PER_SECOND UINT64_C(1000000000)
#define NANOSECONDS_PER_MILLISECOND UINT64_C(1000000)

static bool valid_time(const struct timespec *value)
{
    return value != NULL && value->tv_sec >= 0 && value->tv_nsec >= 0 &&
           value->tv_nsec < (long)NANOSECONDS_PER_SECOND;
}

static int compare_time(const struct timespec *left,
                        const struct timespec *right)
{
    if (left->tv_sec != right->tv_sec) {
        return left->tv_sec < right->tv_sec ? -1 : 1;
    }
    if (left->tv_nsec != right->tv_nsec) {
        return left->tv_nsec < right->tv_nsec ? -1 : 1;
    }
    return 0;
}

static bool add_nanoseconds(struct timespec *value, uint64_t nanoseconds)
{
    const uint64_t added_seconds = nanoseconds / NANOSECONDS_PER_SECOND;
    const uint64_t added_nanoseconds = nanoseconds % NANOSECONDS_PER_SECOND;
    const uint64_t current_seconds = (uint64_t)value->tv_sec;
    uint64_t next_seconds;
    uint64_t next_nanoseconds = (uint64_t)value->tv_nsec + added_nanoseconds;

    if (next_nanoseconds >= NANOSECONDS_PER_SECOND) {
        next_nanoseconds -= NANOSECONDS_PER_SECOND;
        if (added_seconds == UINT64_MAX) {
            return false;
        }
        next_seconds = added_seconds + 1;
    } else {
        next_seconds = added_seconds;
    }
    if (next_seconds > (uint64_t)INT64_MAX ||
        current_seconds > (uint64_t)INT64_MAX - next_seconds) {
        return false;
    }
    value->tv_sec = (time_t)(current_seconds + next_seconds);
    value->tv_nsec = (long)next_nanoseconds;
    return true;
}

static bool elapsed_nanoseconds(const struct timespec *later,
                                const struct timespec *earlier,
                                uint64_t *elapsed)
{
    uint64_t seconds = (uint64_t)(later->tv_sec - earlier->tv_sec);
    uint64_t nanoseconds;

    if (later->tv_nsec >= earlier->tv_nsec) {
        nanoseconds = (uint64_t)(later->tv_nsec - earlier->tv_nsec);
    } else {
        if (seconds == 0) {
            return false;
        }
        --seconds;
        nanoseconds = NANOSECONDS_PER_SECOND -
                      (uint64_t)(earlier->tv_nsec - later->tv_nsec);
    }
    if (seconds > (UINT64_MAX - nanoseconds) / NANOSECONDS_PER_SECOND) {
        return false;
    }
    *elapsed = seconds * NANOSECONDS_PER_SECOND + nanoseconds;
    return true;
}

int sampling_scheduler_init(struct sampling_scheduler *scheduler,
                            unsigned int interval_ms,
                            const struct timespec *start_time)
{
    if (scheduler == NULL || !valid_time(start_time) ||
        interval_ms < SENTINEL_MIN_SAMPLING_INTERVAL_MS ||
        interval_ms > SENTINEL_MAX_SAMPLING_INTERVAL_MS) {
        return -1;
    }

    scheduler->interval_ms = interval_ms;
    scheduler->deadline = *start_time;
    return 0;
}

int sampling_scheduler_next_deadline(struct sampling_scheduler *scheduler,
                                     const struct timespec *now,
                                     struct timespec *deadline)
{
    struct timespec next;
    uint64_t interval_nanoseconds;
    uint64_t elapsed;

    if (scheduler == NULL || deadline == NULL || !valid_time(now) ||
        scheduler->interval_ms < SENTINEL_MIN_SAMPLING_INTERVAL_MS ||
        scheduler->interval_ms > SENTINEL_MAX_SAMPLING_INTERVAL_MS ||
        !valid_time(&scheduler->deadline)) {
        return -1;
    }

    interval_nanoseconds =
        (uint64_t)scheduler->interval_ms * NANOSECONDS_PER_MILLISECOND;
    next = scheduler->deadline;
    if (!add_nanoseconds(&next, interval_nanoseconds)) {
        return -1;
    }

    if (compare_time(&next, now) <= 0) {
        uint64_t ticks_to_skip;
        uint64_t advance;

        if (!elapsed_nanoseconds(now, &next, &elapsed)) {
            return -1;
        }
        ticks_to_skip = elapsed / interval_nanoseconds + 1;
        if (ticks_to_skip > UINT64_MAX / interval_nanoseconds) {
            return -1;
        }
        advance = ticks_to_skip * interval_nanoseconds;
        if (!add_nanoseconds(&next, advance)) {
            return -1;
        }
    }

    scheduler->deadline = next;
    *deadline = next;
    return 0;
}
