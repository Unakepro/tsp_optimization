cmake_minimum_required(VERSION 3.16)

project(tsp_optimization LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_executable(tsp_optimization main.cpp)

add_executable(tsp_hyperparameter_search hyperparameter_search/hyperparameter_search.cpp)
target_compile_definitions(tsp_hyperparameter_search PRIVATE
    TSP_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
)

foreach(target tsp_optimization tsp_hyperparameter_search)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endforeach()
