#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sentinel/collector.h"
#include "sentinel/cpu_collector.h"
#include "sentinel/memory_collector.h"
#include "sentinel/version.h"

static volatile sig_atomic_t keep_running = 1;

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
                           size_t collector_count)
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
            fprintf(stderr, "sentineld: %s collector: %s\n",
                    collectors[i].name, error);
        }
    }
    (void)fflush(stdout);
}

int main(int argc, char **argv)
{
    const char *config_path = SENTINEL_DEFAULT_CONFIG_PATH;
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

    if (install_signal_handlers() == -1) {
        fprintf(stderr, "sentineld: unable to install signal handlers: %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    cpu_collector_init(&collectors[0], &cpu_state, NULL);
    memory_collector_init(&collectors[1], &memory_state, NULL);

    fprintf(stderr, "sentineld: starting with configuration %s\n", config_path);
    fprintf(stderr, "sentineld: collecting CPU and memory telemetry\n");

    while (keep_running) {
        const struct timespec interval = {.tv_sec = 1, .tv_nsec = 0};

        run_collectors(collectors, sizeof(collectors) / sizeof(collectors[0]));
        if (keep_running) {
            (void)nanosleep(&interval, NULL);
        }
    }

    fprintf(stderr, "sentineld: shutting down\n");
    return EXIT_SUCCESS;
}
