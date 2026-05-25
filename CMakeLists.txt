cmake_minimum_required(VERSION 3.10)

project(dfnd)

find_package(raylib CONFIG REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(dfnd)

target_sources(dfnd PRIVATE main.c)

target_link_libraries(dfnd PRIVATE raylib)
