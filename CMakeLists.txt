# CMakeLists.txt — portable build for the AV Perception Evaluation Platform.
#
# The reference build on the author's machine is the plain Makefile (CMake is
# not installed there); this file is provided for portability. It builds the
# `pev` binary and a `pev_tests` target registered with CTest.
#
#   cmake -S . -B build && cmake --build build
#   ctest --test-dir build
#
# Author: Aditya Kopalli, 2026. MIT licensed.

cmake_minimum_required(VERSION 3.16)
project(perception_eval_platform CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(Threads REQUIRED)

set(PEV_LIB_SOURCES
  src/assignment.cpp
  src/dataset.cpp
  src/evaluator.cpp
  src/generator.cpp
  src/geometry.cpp
  src/io_util.cpp
  src/matching.cpp
  src/metrics.cpp
  src/report.cpp
  src/taxonomy.cpp
  src/tracking.cpp
  src/triage.cpp)

add_library(pev_core STATIC ${PEV_LIB_SOURCES})
target_include_directories(pev_core PUBLIC include)
target_link_libraries(pev_core PUBLIC Threads::Threads)

add_executable(pev src/main.cpp)
target_link_libraries(pev PRIVATE pev_core)

enable_testing()
add_executable(pev_tests tests/test_main.cpp)
target_link_libraries(pev_tests PRIVATE pev_core)
add_test(NAME pev_tests COMMAND pev_tests)
