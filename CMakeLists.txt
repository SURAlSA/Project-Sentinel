cmake_minimum_required(VERSION 3.16)

project(sentinel
    VERSION 0.1.0
    DESCRIPTION "Lightweight autonomous telemetry and health monitoring for Linux"
    LANGUAGES C
)

include(CTest)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wall>
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wpedantic>
)

add_subdirectory(daemon)