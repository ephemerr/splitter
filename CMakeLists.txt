cmake_minimum_required(VERSION 3.10)

project(splitter_project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE sources      src/*.cpp src/*.h)
file(GLOB_RECURSE sources_test test/*.cpp test/*.hpp)

add_executable(splitter_test ${sources} ${sources_test})

target_include_directories(splitter_test PUBLIC src test)
