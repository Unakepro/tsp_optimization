cmake_minimum_required(VERSION 3.21)

project(tsp_optimization LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_library(tsp_core
    Cities/city.cpp
    sa/sim_an.cpp
    genetic/genetic.cpp
    aco/aco.cpp
)

add_executable(tsp_optimization main.cpp)
target_link_libraries(tsp_optimization PRIVATE tsp_core)
target_compile_definitions(tsp_optimization PRIVATE
    TSP_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

add_executable(tsp_hyperparameter_search hyperparameter_search/hyperparameter_search.cpp)
target_link_libraries(tsp_hyperparameter_search PRIVATE tsp_core)
target_compile_definitions(tsp_hyperparameter_search PRIVATE
    TSP_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

if(BUILD_TESTING)
    add_executable(tsp_tests tests/tsp_tests.cpp)
    target_link_libraries(tsp_tests PRIVATE tsp_core)
    add_test(NAME tsp_tests COMMAND tsp_tests)
endif()

set(TSP_TARGETS tsp_core tsp_optimization tsp_hyperparameter_search)
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
