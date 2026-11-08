add_library(sentinel_common
    src/telemetry.c
)

target_include_directories(sentinel_common
    PUBLIC
        ${PROJECT_SOURCE_DIR}/common/include
)
