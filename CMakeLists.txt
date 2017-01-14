cmake_minimum_required(VERSION 3.6)
project(FileSystem)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES FileSystem.h FileSystem.c)
add_executable(FileSystem ${SOURCE_FILES})