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
