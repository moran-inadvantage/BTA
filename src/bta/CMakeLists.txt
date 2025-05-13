cmake_minimum_required(VERSION 3.14)

file(GLOB_RECURSE BTA_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/devices/*.cpp
)


add_library(BTA ${BTA_SOURCES})

target_link_libraries(BTA PUBLIC platform)

target_include_directories(BTA
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/devices
)