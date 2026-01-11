cmake_minimum_required(VERSION 3.31.6)
project(main C)

set(CMAKE_C_STANDARD 11)

add_executable(main
        src/main.c
        src/cores.c
        src/mwc.c
)

target_include_directories(main PRIVATE include)
