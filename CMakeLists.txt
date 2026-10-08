cmake_minimum_required(VERSION 3.20)

project(Cadence VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CADENCE_WARNINGS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

option(CADENCE_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(CADENCE_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(cadence_core
    src/task_state.cpp
    src/operation.cpp
    src/task.cpp
    src/ready_queue.cpp
    src/event.cpp
    src/engine.cpp
    src/metrics.cpp
    src/validate.cpp
    src/scenario.cpp
)

target_include_directories(cadence_core
    PUBLIC
        include
)

target_compile_options(cadence_core
    PRIVATE
        ${CADENCE_WARNINGS}
)

add_library(cadence_json_adapter src/scenario_json.cpp)
target_include_directories(cadence_json_adapter PUBLIC include)
target_compile_options(cadence_json_adapter PRIVATE ${CADENCE_WARNINGS})
target_link_libraries(cadence_json_adapter PUBLIC cadence_core)

add_executable(cadence
    src/main.cpp
)

target_link_libraries(cadence
    PRIVATE
        cadence_json_adapter
)

target_compile_options(cadence
    PRIVATE
        ${CADENCE_WARNINGS}
)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(googletest)

enable_testing()

include(GoogleTest)

add_executable(cadence_tests
    tests/test_task_state.cpp
    tests/test_operation.cpp
    tests/test_task.cpp
    tests/test_ready_queue.cpp
    tests/test_engine.cpp
    tests/test_event.cpp
    tests/test_metrics.cpp
    tests/test_validate.cpp
    tests/test_continuation.cpp
    tests/test_determinism.cpp
    tests/test_s1.cpp
    tests/test_scenario_json.cpp
)

target_link_libraries(cadence_tests
    PRIVATE
        cadence_json_adapter
        GTest::gtest_main
)

target_compile_options(cadence_tests
    PRIVATE
        ${CADENCE_WARNINGS}
)

gtest_discover_tests(cadence_tests)
