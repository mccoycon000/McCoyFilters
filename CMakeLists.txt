cmake_minimum_required(VERSION 3.26)
project(McCoyFilters C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

add_executable(McCoyFilters main.c)
