#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "sentinel/collector.h"
#include "sentinel/config.h"
#include "sentinel/cpu_collector.h"
#include "sentinel/memory_collector.h"
#include "sentinel/scheduler.h"
#include "sentinel/version.h"

static volatile sig_atomic_t keep_running = 1;

static void log_message(enum sentinel_log_level configured_level,
                        enum sentinel_log_level message_level,
                        const char *format,
                        ...)
{
    va_list arguments;

    if (message_level < configured_level) {
        return;
    }

    fprintf(stderr, "sentineld: %s: ", sentinel_log_level_name(message_level));
    va_start(arguments, format);
    vfprintf(stderr, format, arguments);
    va_end(arguments);
    fputc('\n', stderr);
}

static void handle_signal(int signal_number)
{
    (void)signal_number;
    keep_running = 0;
}

static void print_usage(const char *program)
{
    printf("Usage: %s [--config PATH] [--version] [--help]\n", program);
}

static int install_signal_handlers(void)
{
    struct sigaction action = {
        .sa_handler = handle_signal,
    };

    if (sigemptyset(&action.sa_mask) == -1) {
        return -1;
    }

    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGTERM, &action, NULL) == -1) {
        return -1;
    }

    return 0;
}

static int wait_for_next_sample(struct sampling_scheduler *scheduler)
{
    sigset_t shutdown_signals;
    sigset_t previous_mask;
    sigset_t wait_mask;
    struct timespec now;
    struct timespec deadline;
    int wait_error = 0;

    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
        return -1;
    }
    if (sampling_scheduler_next_deadline(scheduler, &now, &deadline) == -1) {
        errno = EINVAL;
        return -1;
    }

    if (sigemptyset(&shutdown_signals) == -1 ||
        sigaddset(&shutdown_signals, SIGINT) == -1 ||
        sigaddset(&shutdown_signals, SIGTERM) == -1 ||
        sigprocmask(SIG_BLOCK, &shutdown_signals, &previous_mask) == -1) {
        return -1;
    }
    /*
     * Keep shutdown signals blocked while checking the flag. pselect atomically
     * unmasks them while waiting, so a signal cannot arrive in a check/wait gap.
     */
    wait_mask = previous_mask;
    if (sigdelset(&wait_mask, SIGINT) == -1 ||
        sigdelset(&wait_mask, SIGTERM) == -1) {
        const int saved_errno = errno;
        (void)sigprocmask(SIG_SETMASK, &previous_mask, NULL);
        errno = saved_errno;
        return -1;
    }

    while (keep_running) {
        struct timespec remaining;
        int result;

        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
            wait_error = errno;
            break;
        }
        if (now.tv_sec > deadline.tv_sec ||
            (now.tv_sec == deadline.tv_sec &&
             now.tv_nsec >= deadline.tv_nsec)) {
            break;
        }
        remaining.tv_sec = deadline.tv_sec - now.tv_sec;
        remaining.tv_nsec = deadline.tv_nsec - now.tv_nsec;
        if (remaining.tv_nsec < 0) {
            --remaining.tv_sec;
            remaining.tv_nsec += 1000000000L;
        }

        result = pselect(0, NULL, NULL, NULL, &remaining, &wait_mask);
        if (result == -1 && errno != EINTR) {
            wait_error = errno;
            break;
        }
    }

    if (sigprocmask(SIG_SETMASK, &previous_mask, NULL) == -1) {
        return -1;
    }
    if (wait_error != 0) {
        errno = wait_error;
        return -1;
    }
    return 0;
}

static void run_collectors(const struct telemetry_collector *collectors,
                           size_t collector_count,
                           enum sentinel_log_level log_level)
{
    for (size_t i = 0; i < collector_count; ++i) {
        struct telemetry_sample sample;
        char error[256];
        const enum collector_result result =
            telemetry_collector_collect(&collectors[i], &sample, error,
                                        sizeof(error));

        if (result == COLLECTOR_SUCCESS) {
            printf("sample timestamp_ms=%" PRId64 " metric=%s value=%.2f "
                   "unit=%s source=%s\n",
                   sample.timestamp_ms, sample.metric, sample.value,
                   sample.unit, sample.source != NULL ? sample.source : "none");
        } else if (result == COLLECTOR_FAILURE) {
            log_message(log_level, SENTINEL_LOG_ERROR, "%s collector: %s",
                        collectors[i].name, error);
        }
    }
    (void)fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *config_path = SENTINEL_DEFAULT_CONFIG_PATH;
    struct sentinel_config config;
    char config_error[512];
    struct cpu_collector_state cpu_state;
    struct memory_collector_state memory_state;
    struct sampling_scheduler scheduler;
    struct telemetry_collector collectors[2];
    struct timespec scheduler_start;
    static const struct option options[] = {
        {"config", required_argument, NULL, 'c'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0},
    };
    int option;

    while ((option = getopt_long(argc, argv, "c:hv", options, NULL)) != -1) {
        switch (option) {
        case 'c':
            config_path = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case 'v':
            printf("%s %s\n", SENTINEL_NAME, SENTINEL_VERSION);
            return EXIT_SUCCESS;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "sentineld: unexpected argument: %s\n", argv[optind]);
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (sentinel_config_load(config_path, &config, config_error,
                             sizeof(config_error)) == -1) {
        fprintf(stderr, "sentineld: configuration error: %s\n", config_error);
        return EXIT_FAILURE;
    }

    if (install_signal_handlers() == -1) {
        fprintf(stderr, "sentineld: unable to install signal handlers: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &scheduler_start) == -1) {
        fprintf(stderr, "sentineld: unable to initialize sampling scheduler: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }
    if (sampling_scheduler_init(&scheduler, config.sampling_interval_ms,
                                &scheduler_start) == -1) {
        fprintf(stderr, "sentineld: invalid sampling scheduler configuration\n");
        return EXIT_FAILURE;
    }

    cpu_collector_init(&collectors[0], &cpu_state, NULL);
    memory_collector_init(&collectors[1], &memory_state, NULL);

    log_message(config.log_level, SENTINEL_LOG_DEBUG,
                "loaded configuration %s", config_path);
    /* Always show effective settings, even when informational logs are muted. */
    fprintf(stderr,
            "sentineld: configuration: sampling_interval_ms=%u database_path=\"%s\" "
            "log_level=%s enable_remote_telemetry=%s\n",
            config.sampling_interval_ms, config.database_path,
            sentinel_log_level_name(config.log_level),
            config.enable_remote_telemetry ? "true" : "false");
    if (config.enable_remote_telemetry) {
        log_message(config.log_level, SENTINEL_LOG_WARN,
                    "remote telemetry is configured but not implemented");
    }
    log_message(config.log_level, SENTINEL_LOG_INFO,
                "collecting CPU and memory telemetry");

    while (keep_running) {
        run_collectors(collectors, sizeof(collectors) / sizeof(collectors[0]),
                       config.log_level);
        if (keep_running && wait_for_next_sample(&scheduler) == -1) {
            log_message(config.log_level, SENTINEL_LOG_ERROR,
                        "sampling scheduler failed: %s", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    log_message(config.log_level, SENTINEL_LOG_INFO, "shutting down");
    return EXIT_SUCCESS;
}
