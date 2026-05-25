cmake_minimum_required(VERSION 3.20)
project(CSE305_project)

set(CMAKE_CXX_STANDARD 20)

if(NO_LOGS)
    add_compile_definitions(NO_LOGS)
endif()

add_executable(CSE305_project
        main.cpp
        src/Master.cpp
        src/Worker.cpp
        src/VisitedSet.cpp
        src/RecursivelyEnumeratedSet.cpp
        tests/sudoku.cpp
)

target_include_directories(CSE305_project PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/simple-cpp-logger/include
)