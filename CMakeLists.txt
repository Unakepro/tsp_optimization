cmake_minimum_required(VERSION 3.16)

project(tsp_optimization LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

add_executable(tsp_optimization main.cpp)

target_compile_options(tsp_optimization PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
