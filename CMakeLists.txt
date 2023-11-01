cmake_minimum_required(VERSION 3.20)

project(lume)

set(CMAKE_CXX_STANDARD 20)

set(SOURCE_FILES
        src/main.cpp)

add_executable(lume)

target_sources(lume PRIVATE ${SOURCE_FILES})

add_executable(tests)

target_sources(tests PRIVATE tests/tests.cpp)

add_dependencies(tests lume)

