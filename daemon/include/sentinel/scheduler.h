#ifndef SENTINEL_SCHEDULER_H
#define SENTINEL_SCHEDULER_H

#include <time.h>

struct sampling_scheduler {
    unsigned int interval_ms;
    struct timespec deadline;
};

/*
 * Start a fixed-interval schedule at start_time. The caller normally collects
 * immediately, then asks for the next deadline after each collection.
 */
int sampling_scheduler_init(struct sampling_scheduler *scheduler,
                            unsigned int interval_ms,
                            const struct timespec *start_time);

/*
 * Advance the schedule from its previous deadline. If collection ran late,
 * missed ticks are skipped so the daemon does not perform catch-up bursts.
 */
int sampling_scheduler_next_deadline(struct sampling_scheduler *scheduler,
                                     const struct timespec *now,
                                     struct timespec *deadline);

#endif
