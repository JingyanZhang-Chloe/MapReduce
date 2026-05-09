cmake_minimum_required(VERSION 3.28)
project(CSE305_project)

set(CMAKE_CXX_STANDARD 20)

add_executable(CSE305_project
        main.cpp
)

target_include_directories(CSE305_project PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)