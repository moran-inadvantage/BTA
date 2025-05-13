cmake_minimum_required(VERSION 3.14)

file(GLOB_RECURSE PLATFORM_CODE
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)


add_library(platform ${PLATFORM_CODE})

target_include_directories(platform 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/interfaces
)