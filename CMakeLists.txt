cmake_minimum_required(VERSION 3.20)

project(Cadence VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CADENCE_WARNINGS -Wall -Wextra -Wpedantic -Werror)

# Core library
add_library(cadence_core
    src/task_state.cpp
    src/operation.cpp
    src/task.cpp
    src/ready_queue.cpp
    src/engine.cpp
)

target_include_directories(cadence_core PUBLIC include)
target_compile_options(cadence_core PRIVATE ${CADENCE_WARNINGS})

# Main executable
add_executable(cadence
    src/main.cpp
)

target_link_libraries(cadence PRIVATE cadence_core)
target_compile_options(cadence PRIVATE ${CADENCE_WARNINGS})

# GoogleTest
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(googletest)

enable_testing()

# Tests
add_executable(cadence_tests
    tests/test_task_state.cpp
    tests/test_operation.cpp
    tests/test_task.cpp
    tests/test_ready_queue.cpp
    tests/test_engine.cpp
)

target_link_libraries(cadence_tests PRIVATE
    cadence_core
    GTest::gtest_main
)

target_compile_options(cadence_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

include(GoogleTest)

gtest_discover_tests(cadence_tests)
