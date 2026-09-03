cmake_minimum_required(VERSION 3.24)
project(markdawn LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt's meta-object tooling. Needed project-wide once QObject-derived classes
# show up (Phase 1+); harmless to enable now.
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# One place to find the built executable regardless of platform/preset,
# instead of the per-target default (build/<preset>/markdawn-app/markdawn).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Warnings-as-errors from day one (Phase 0 requirement, §5). INTERFACE target
# so every subdirectory opts in with one target_link_libraries line instead
# of repeating compiler-specific flags.
add_library(markdawn_warnings INTERFACE)
if(MSVC)
    target_compile_options(markdawn_warnings INTERFACE /W4 /WX)
else()
    target_compile_options(markdawn_warnings INTERFACE -Wall -Wextra -Werror)
endif()

add_subdirectory(core-lib)
add_subdirectory(markdawn-app)

# Phase 3 is the first phase with a genuine headless unit test (TocModel's
# hierarchy-building logic), so tests/ is wired in starting here.
enable_testing()
add_subdirectory(tests/core-lib)
