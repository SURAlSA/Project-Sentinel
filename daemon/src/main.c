#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sentinel/collector.h"
#include "sentinel/config.h"
#include "sentinel/cpu_collector.h"
#include "sentinel/memory_collector.h"
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
            printf("%s=%.2f %s\n", sample.metric, sample.value, sample.unit);
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
    struct telemetry_collector collectors[2];
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
        const struct timespec interval = {
            .tv_sec = config.sampling_interval_ms / 1000,
            .tv_nsec = (long)(config.sampling_interval_ms % 1000) * 1000000L,
        };

        run_collectors(collectors, sizeof(collectors) / sizeof(collectors[0]),
                       config.log_level);
        if (keep_running) {
            while (nanosleep(&interval, NULL) == -1 && errno == EINTR &&
                   keep_running) {
            }
        }
    }

    log_message(config.log_level, SENTINEL_LOG_INFO, "shutting down");
    return EXIT_SUCCESS;
}
