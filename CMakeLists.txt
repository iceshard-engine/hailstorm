cmake_minimum_required(VERSION 3.15)
project(iceshard_hailstorm CXX)

add_library(hailstorm
    private/hailstorm_operations.cxx
    private/hailstorm.cxx
)

target_include_directories(hailstorm PUBLIC public)

set_property(TARGET hailstorm PROPERTY CXX_STANDARD 20)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/public/"
    DESTINATION "public"
    FILES_MATCHING
    PATTERN "*.hxx"
)

install(TARGETS hailstorm)
