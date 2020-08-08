cmake_minimum_required(VERSION 3.10)

project(splitter_project)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB_RECURSE sources      src/*.cpp src/*.h)
file(GLOB_RECURSE sources_test test/*.cpp test/*.hpp)

add_executable(splitter_test ${sources} ${sources_test})

target_include_directories(splitter_test PUBLIC src test)

# add_compile_options(-pthread)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(splitter_test PRIVATE Threads::Threads)
