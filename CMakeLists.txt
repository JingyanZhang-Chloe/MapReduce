cmake_minimum_required(VERSION 3.20)
project(CSE305_project)

set(CMAKE_CXX_STANDARD 20)

add_executable(basic_test
        main.cpp
)

add_executable(graph_exploration_test
        tests/graph-exploration.cpp
)

add_executable(sudoku_test
        tests/sudoku.cpp
)

target_include_directories(basic_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(graph_exploration_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(sudoku_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)