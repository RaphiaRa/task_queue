cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
project(test_tq)

set(CMAKE_C_STANDARD 99)

# Extra warnings, treat warnings as error
if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")
  add_compile_options(-Wall -Wextra -pedantic -Werror)
endif()

add_library(tq 
    tq.c
)

target_include_directories(tq PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)

add_library(tq::tq ALIAS tq)

if (NOT TQ_DISABLE_TESTS)
    add_executable(test_tq
        test_tq.c
    )
    target_link_libraries(test_tq PUBLIC 
        tq::tq
    )
endif()
