#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static int failures;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,  \
                    #condition);                                               \
            ++failures;                                                        \
        }                                                                       \
    } while (0)

static bool create_config(char path[])
{
    const char template_path[] = "/tmp/sentinel-lifecycle-XXXXXX";
    int descriptor;
    FILE *file;

    memcpy(path, template_path, sizeof(template_path));
    descriptor = mkstemp(path);
    if (descriptor == -1) {
        return false;
    }
    file = fdopen(descriptor, "w");
    if (file == NULL) {
        (void)close(descriptor);
        (void)unlink(path);
        return false;
    }
    if (fputs("sampling_interval_ms = 100\n"
              "database_path = \"/tmp/sentinel-test.db\"\n"
              "log_level = \"error\"\n"
              "enable_remote_telemetry = false\n",
              file) == EOF) {
        (void)fclose(file);
        (void)unlink(path);
        return false;
    }
    if (fclose(file) == EOF) {
        (void)unlink(path);
        return false;
    }
    return true;
}

static size_t count_memory_samples(const char *text)
{
    const char *cursor = text;
    size_t count = 0;

    while ((cursor = strstr(cursor, "metric=memory.utilization")) != NULL) {
        ++count;
        ++cursor;
    }
    return count;
}

static void test_daemon_samples_and_stops_on_signal(const char *daemon_path,
                                                    int shutdown_signal)
{
    char config_path[64];
    int output_pipe[2];
    pid_t child;
    char output[8192] = {0};
    size_t output_length = 0;
    bool saw_repeated_samples = false;
    bool child_reaped = false;
    int child_status = 0;

    CHECK(create_config(config_path));
    if (failures != 0) {
        return;
    }
    CHECK(pipe(output_pipe) == 0);
    if (failures != 0) {
        (void)unlink(config_path);
        return;
    }

    child = fork();
    CHECK(child != -1);
    if (child == -1) {
        (void)close(output_pipe[0]);
        (void)close(output_pipe[1]);
        (void)unlink(config_path);
        return;
    }
    if (child == 0) {
        (void)close(output_pipe[0]);
        if (dup2(output_pipe[1], STDOUT_FILENO) == -1 ||
            dup2(output_pipe[1], STDERR_FILENO) == -1) {
            _exit(126);
        }
        (void)close(output_pipe[1]);
        execl(daemon_path, daemon_path, "--config", config_path,
              (char *)NULL);
        _exit(127);
    }

    (void)close(output_pipe[1]);
    for (int attempt = 0; attempt < 40; ++attempt) {
        struct pollfd descriptor = {
            .fd = output_pipe[0],
            .events = POLLIN,
        };
        const int poll_result = poll(&descriptor, 1, 100);

        if (poll_result == -1 && errno == EINTR) {
            continue;
        }
        if (poll_result <= 0) {
            continue;
        }
        if ((descriptor.revents & POLLIN) != 0 &&
            output_length + 1 < sizeof(output)) {
            const ssize_t bytes_read =
                read(output_pipe[0], output + output_length,
                     sizeof(output) - output_length - 1);

            if (bytes_read > 0) {
                output_length += (size_t)bytes_read;
                output[output_length] = '\0';
                if (count_memory_samples(output) >= 2) {
                    saw_repeated_samples = true;
                    break;
                }
            } else if (bytes_read == 0) {
                break;
            }
        } else if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            break;
        }
    }
    CHECK(saw_repeated_samples);
    CHECK(strstr(output, "sample timestamp_ms=") != NULL);
    CHECK(strstr(output, "unit=% source=none") != NULL);

    CHECK(kill(child, shutdown_signal) == 0);
    for (int attempt = 0; attempt < 40; ++attempt) {
        const pid_t wait_result = waitpid(child, &child_status, WNOHANG);

        if (wait_result == child) {
            child_reaped = true;
            break;
        }
        if (wait_result == -1 && errno != EINTR) {
            break;
        }
        {
            const struct timespec pause = {.tv_sec = 0, .tv_nsec = 50000000};
            (void)nanosleep(&pause, NULL);
        }
    }
    if (!child_reaped) {
        (void)kill(child, SIGKILL);
        (void)waitpid(child, &child_status, 0);
    }
    CHECK(child_reaped);
    if (child_reaped) {
        CHECK(WIFEXITED(child_status));
        if (WIFEXITED(child_status)) {
            CHECK(WEXITSTATUS(child_status) == 0);
        }
    }

    (void)close(output_pipe[0]);
    CHECK(unlink(config_path) == 0);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s PATH_TO_SENTINELD\n", argv[0]);
        return 2;
    }

    test_daemon_samples_and_stops_on_signal(argv[1], SIGINT);
    test_daemon_samples_and_stops_on_signal(argv[1], SIGTERM);
    if (failures != 0) {
        fprintf(stderr, "%d daemon lifecycle test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
