cmake_minimum_required(VERSION 3.28)
project(CSE305_project)

set(CMAKE_CXX_STANDARD 20)

add_executable(CSE305_project
        main.cpp
        include/VisitedSet.hpp
        include/RecursivelyEnumeratedSet.hpp
        include/Master.hpp
        include/Worker.hpp
)

target_include_directories(CSE305_project PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)