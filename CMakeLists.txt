cmake_minimum_required(VERSION 3.0)

project(fl)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

set(FL_HEADER_FILES
    ${CMAKE_CURRENT_LIST_DIR}/fl.hpp)

add_subdirectory(tst)
