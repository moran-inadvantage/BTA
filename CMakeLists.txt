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

add_subdirectory(src)

# Run with cmake -B build -DENABLE_TESTS=ON && make -C build
option(ENABLE_TESTS "Enable building of tests" OFF)
# Only add tests if ENABLE_TESTS is ON
if(ENABLE_TESTS STREQUAL ON)
#    enable_testing()
#    add_subdirectory(External/googletest)
#    add_subdirectory(test)
endif()

# Add executable
add_executable(BTAudioCard ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
# Link libraries
target_link_libraries(BTAudioCard BTA)
