cmake_minimum_required(VERSION 3.15)
project(iceshard_hailstorm CXX)

add_library(hailstorm
    private/hailstorm_operations.cxx
    private/hailstorm.cxx
)

target_include_directories(hailstorm PUBLIC public)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/public"
    DESTINATION "include"
    FILES_MATCHING
    PATTERN "*.hxx"
)

install(TARGETS hailstorm)
