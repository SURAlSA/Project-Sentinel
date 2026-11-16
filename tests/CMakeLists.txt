add_executable(test_telemetry
    test_telemetry.c
)

target_link_libraries(test_telemetry
    PRIVATE
        sentinel_common
)

add_test(NAME telemetry_sample COMMAND test_telemetry)

add_executable(test_collectors
    test_collectors.c
)

target_link_libraries(test_collectors
    PRIVATE
        sentinel_collectors
)

add_test(NAME collectors COMMAND test_collectors)

add_executable(test_config
    test_config.c
)

target_link_libraries(test_config
    PRIVATE
        sentinel_collectors
)

add_test(NAME config COMMAND test_config)

add_executable(test_scheduler
    test_scheduler.c
)

target_link_libraries(test_scheduler
    PRIVATE
        sentinel_collectors
)

add_test(NAME scheduler COMMAND test_scheduler)

add_executable(test_daemon_lifecycle
    test_daemon_lifecycle.c
)

add_test(NAME daemon_lifecycle
    COMMAND test_daemon_lifecycle $<TARGET_FILE:sentineld>
)
