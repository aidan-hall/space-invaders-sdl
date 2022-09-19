# CMake.
cmake_minimum_required(VERSION 3.15)

# Project.
project(Game VERSION 0.1)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

# C Standard.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Executable
add_executable(Game src/tecs.cpp src/sdl.cpp src/main.cpp src/alien_movement_system.cpp)

# Includes

target_include_directories(Game PRIVATE "${CMAKE_SOURCE_DIR}/include")

# Files.
# Libraries.
find_package(SDL2 REQUIRED)
target_link_libraries(Game PRIVATE SDL2::SDL2)
target_link_libraries(Game PRIVATE SDL2_image)
target_link_libraries(Game PRIVATE SDL2_ttf)
target_link_libraries(Game PRIVATE SDL2_mixer)

add_subdirectory("${CMAKE_SOURCE_DIR}/external/glm")
target_link_libraries(Game PRIVATE glm)

# Installation.
target_compile_options(Game PRIVATE -Wpedantic -Wall -Wextra -Wimplicit-fallthrough -Werror -g)
file(CREATE_LINK "${CMAKE_BINARY_DIR}/compile_commands.json" "${CMAKE_SOURCE_DIR}/compile_commands.json" SYMBOLIC)
