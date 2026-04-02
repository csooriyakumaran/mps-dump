cmake_minimum_required(VERSION 3.25)

# Project configuration
project(mps-dump C)

# Use clang as the C compiler
if (NOT CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER clang)
endif()

# Require C11 standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Add external scanivalve-mps-protocol interface library
add_subdirectory(external/scanivalve-mps-protocol)

# Main executable
add_executable(mps-dump
    src/main.c
)

# Link against scanivalve-mps-protocol if needed later
# target_link_libraries(mps-dump PRIVATE scanivalve-mps-protocol)

