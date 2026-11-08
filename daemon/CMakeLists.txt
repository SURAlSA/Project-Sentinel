add_library(sentinel_collectors
    src/collector.c
    src/config.c
    src/cpu_collector.c
    src/memory_collector.c
)

target_include_directories(sentinel_collectors
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/common/include
)

target_link_libraries(sentinel_collectors
    PRIVATE
        sentinel_common
)

add_executable(sentineld
    src/main.c
)

target_include_directories(sentineld
    PRIVATE
        ${PROJECT_SOURCE_DIR}/common/include
)

target_link_libraries(sentineld
    PRIVATE
        sentinel_collectors
)

target_compile_definitions(sentineld
    PRIVATE
        SENTINEL_DEFAULT_CONFIG_PATH="/etc/sentinel/sentinel.toml"
)

install(TARGETS sentineld
    RUNTIME DESTINATION bin
)

if(BUILD_TESTING)
    add_test(NAME sentineld_version COMMAND sentineld --version)
endif()