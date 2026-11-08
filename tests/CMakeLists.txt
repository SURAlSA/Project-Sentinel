add_executable(test_telemetry
    test_telemetry.c
)

target_link_libraries(test_telemetry
    PRIVATE
        sentinel_common
)

add_test(NAME telemetry_sample COMMAND test_telemetry)
