add_executable(sentineld
    src/main.c
)

target_include_directories(sentineld
    PRIVATE
        ${PROJECT_SOURCE_DIR}/common/include
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