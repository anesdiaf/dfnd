cmake_minimum_required(VERSION 3.10)

project(dfnd)

find_package(raylib CONFIG REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(dfnd)

add_custom_command(TARGET dfnd POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:dfnd>/assets"
    COMMENT "Copying assets to build directory"
)

target_sources(dfnd PRIVATE main.c)

target_link_libraries(dfnd PRIVATE raylib)
