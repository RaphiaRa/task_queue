cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
project(test_tq)

set(CMAKE_C_STANDARD 99)

# Extra warnings, treat warnings as error
if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_executable(test_tq
    test_tq.c
    tq.c
)
