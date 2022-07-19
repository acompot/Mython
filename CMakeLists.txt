cmake_minimum_required(VERSION 3.5)

project(MythonInterpritator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set (headers
    "include/lexer.h"
    "include/parse.h"
    "include/runtime.h"
    "include/statement.h"
    "include/test_runner_p.h")

set(sources
    "src/lexer.cpp"
    "src/lexer_test.cpp"
    "src/parse.cpp"
    "src/parse_test.cpp"
    "src/runtime.cpp"
    "src/runtime.cpp"
    "src/runtime.cpp"
    "src/statement.cpp"
    "src/statement.cpp")


add_executable(${CMAKE_PROJECT_NAME} main.cpp ${headers} ${sources})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "include")




