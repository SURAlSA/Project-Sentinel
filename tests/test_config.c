#define _POSIX_C_SOURCE 200809L

#include "sentinel/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static bool write_config(const char *text, char path[], size_t path_size)
{
    const char template_path[] = "/tmp/sentinel-config-test-XXXXXX";
    const size_t template_length = sizeof(template_path);
    int descriptor;
    FILE *file;

    if (path_size < template_length) {
        return false;
    }
    memcpy(path, template_path, template_length);
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
    if (fputs(text, file) == EOF) {
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

static void test_valid_configuration_and_defaults(void)
{
    char path[64];
    char error[256];
    struct sentinel_config config;
    const char *contents =
        "# Sentinel configuration\n"
        "sampling_interval_ms = 2500\n"
        "database_path = \"/tmp/sentinel=data.db\" # comment after a string\n"
        "log_level = \"debug\"\n"
        "enable_remote_telemetry = true\n";

    CHECK(write_config(contents, path, sizeof(path)));
    CHECK(sentinel_config_load(path, &config, error, sizeof(error)) == 0);
    CHECK(config.sampling_interval_ms == 2500);
    CHECK(strcmp(config.database_path, "/tmp/sentinel=data.db") == 0);
    CHECK(config.log_level == SENTINEL_LOG_DEBUG);
    CHECK(config.enable_remote_telemetry);
    CHECK(error[0] == '\0');
    CHECK(unlink(path) == 0);

    CHECK(write_config("log_level = \"warn\"\n", path, sizeof(path)));
    CHECK(sentinel_config_load(path, &config, error, sizeof(error)) == 0);
    CHECK(config.sampling_interval_ms == 1000);
    CHECK(strcmp(config.database_path, "/var/lib/sentinel/data.db") == 0);
    CHECK(config.log_level == SENTINEL_LOG_WARN);
    CHECK(!config.enable_remote_telemetry);
    CHECK(unlink(path) == 0);
}

static void expect_invalid_config(const char *contents,
                                 const char *expected_error)
{
    char path[64];
    char error[256];
    struct sentinel_config config;

    CHECK(write_config(contents, path, sizeof(path)));
    CHECK(sentinel_config_load(path, &config, error, sizeof(error)) == -1);
    CHECK(strstr(error, expected_error) != NULL);
    CHECK(unlink(path) == 0);
}

static void test_invalid_values_and_syntax(void)
{
    expect_invalid_config("sampling_interval_ms = 99\n",
                          "sampling_interval_ms");
    expect_invalid_config("sampling_interval_ms = 60001\n",
                          "sampling_interval_ms");
    expect_invalid_config("sampling_interval_ms = 1.5\n",
                          "sampling_interval_ms");
    expect_invalid_config("log_level = \"verbose\"\n", "log_level");
    expect_invalid_config("enable_remote_telemetry = yes\n",
                          "enable_remote_telemetry");
    expect_invalid_config("database_path = \"\"\n", "database_path");
    expect_invalid_config("unknown_setting = true\n", "unknown setting");
    expect_invalid_config("log_level = \"info\"\nlog_level = \"debug\"\n",
                          "duplicate setting");
    expect_invalid_config("this is not a setting\n", "expected key = value");
}

static void test_missing_file(void)
{
    char path[] = "/tmp/sentinel-config-missing-XXXXXX";
    char error[256];
    struct sentinel_config config;
    const int descriptor = mkstemp(path);

    CHECK(descriptor != -1);
    if (descriptor == -1) {
        return;
    }
    CHECK(close(descriptor) == 0);
    CHECK(unlink(path) == 0);
    CHECK(sentinel_config_load(path, &config, error, sizeof(error)) == -1);
    CHECK(strstr(error, "cannot open configuration") != NULL);
    CHECK(strstr(error, path) != NULL);
}

int main(void)
{
    test_valid_configuration_and_defaults();
    test_invalid_values_and_syntax();
    test_missing_file();

    if (failures != 0) {
        fprintf(stderr, "%d configuration test(s) failed\n", failures);
        return 1;
    }

    return 0;
}
