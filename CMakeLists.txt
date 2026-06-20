cmake_minimum_required(VERSION 3.21)

project(tsp_optimization LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_library(tsp_core
    core/config.cpp
    core/tsp.cpp
    core/datasets.cpp
    algorithms/sa.cpp
    algorithms/genetic.cpp
    algorithms/aco.cpp
)
target_include_directories(tsp_core PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_definitions(tsp_core PRIVATE TSP_PROJECT_ROOT="${PROJECT_SOURCE_DIR}")

add_executable(tsp_optimizer
    main.cpp
    benchmark/benchmark.cpp
)
target_link_libraries(tsp_optimizer PRIVATE tsp_core)

if(BUILD_TESTING)
    add_executable(tsp_tests tests/tsp_tests.cpp)
    target_link_libraries(tsp_tests PRIVATE tsp_core)
    add_test(NAME tsp_tests COMMAND tsp_tests)
    add_test(NAME cli_rejects_duplicate_flags
        COMMAND tsp_optimizer --benchmark-mode timed --set small --algorithm ga
                              --params default --time-limit 1s --seed 1 --seed 2)
    add_test(NAME cli_rejects_flag_as_value
        COMMAND tsp_optimizer --benchmark-mode timed --set small --algorithm ga
                              --params default --time-limit --seed)
    add_test(NAME cli_rejects_bad_two_opt
        COMMAND tsp_optimizer --benchmark-mode timed --set small --algorithm ga
                              --params default --time-limit 1s --two-opt maybe)
    set_tests_properties(cli_rejects_duplicate_flags cli_rejects_flag_as_value cli_rejects_bad_two_opt
        PROPERTIES WILL_FAIL TRUE)
endif()

set(TSP_TARGETS tsp_core tsp_optimizer)
if(BUILD_TESTING)
    list(APPEND TSP_TARGETS tsp_tests)
endif()

option(TSP_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

foreach(target ${TSP_TARGETS})
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)

    if(TSP_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endforeach()
