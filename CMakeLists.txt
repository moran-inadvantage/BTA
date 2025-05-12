cmake_minimum_required(VERSION 3.10)
project(BTAudioCard)

set(CMAKE_CXX_STANDARD 11)

# Option to choose platform
set(PLATFORM "linux_x86" CACHE STRING "Platform to build for (avds or linux_x86)")

# Validate platform selection
if(NOT PLATFORM STREQUAL "avds" AND NOT PLATFORM STREQUAL "linux_x86")
    message(FATAL_ERROR "Unsupported platform: ${PLATFORM}. Use avds or linux_x86.")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Include common headers
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port/${PLATFORM}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/port/${PLATFORM}/Boost
)

# Gather source files
file(GLOB_RECURSE SOURCES
    src/*.cpp
    src/bta/*.cpp
    src/bta/devices/*.cpp
    src/port/${PLATFORM}/*.cpp
)

add_executable(BTAudioCard ${SOURCES})
