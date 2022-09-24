# CMake.
cmake_minimum_required(VERSION 3.15)

# Project.
project(SpaceInvaders VERSION 0.1)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

# C Standard.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Executable
add_executable(SpaceInvaders src/main.cpp src/alien_movement_system.cpp)

# Includes

target_include_directories(SpaceInvaders PRIVATE "${CMAKE_SOURCE_DIR}/include")

# Files.
# Libraries.

add_subdirectory("${CMAKE_SOURCE_DIR}/external/glm")
target_link_libraries(SpaceInvaders PUBLIC glm)
add_subdirectory("${CMAKE_SOURCE_DIR}/external/sdlpp")
target_link_libraries(SpaceInvaders PUBLIC sdlpp)
add_subdirectory("${CMAKE_SOURCE_DIR}/external/tecs")
target_link_libraries(SpaceInvaders PUBLIC tecs)


# Installation.
target_compile_options(SpaceInvaders PRIVATE
  -Wpedantic
  -Wall
  -Wextra
  -Wimplicit-fallthrough
  $<$<CONFIG:DEBUG>:-g3>
  $<$<CONFIG:DEBUG>:-Og>
  $<$<CONFIG:RELEASE>:-O3>
  $<$<CONFIG:RELEASE>:-Werror>
  -g
)

file(CREATE_LINK "${CMAKE_BINARY_DIR}/compile_commands.json" "${CMAKE_SOURCE_DIR}/compile_commands.json" SYMBOLIC)
