# -----------------------------------------------------------------------------
# Copyright (c) 2024, John Burnell
# -----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.10)
project(Crossline VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE Debug)

set(cl_version "0.1")

# -----------------------------------------------------------------------------
# Initial definitions
# -----------------------------------------------------------------------------
set(cl_cflags)
set(cl_cdefs)
set(cl_install_dir)
set(cl_dir)

list(APPEND cl_sources 
            crossline.cpp
            crossline.h
    )

set(cl_example_sources example.c example2.c example_sql.c)

set_source_files_properties(${cl_example_sources} PROPERTIES LANGUAGE CXX )

add_library(Crossline STATIC ${cl_sources})

add_executable(example-cpp example_cpp.cpp)
target_link_libraries(example-cpp Crossline)

add_executable(example example.c)
target_link_libraries(example Crossline)

