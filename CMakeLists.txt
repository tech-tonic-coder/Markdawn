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

# tests/ is intentionally not added yet — no test target exists until a
# later phase writes the first one (see tests/README.md).

# Phase 0.5 — throwaway technical-spike executables (Markdawn-Roadmap.md
# §5, Phase 0.5). OFF by default so spikes never affect the normal app
# build; CI turns this ON only in the dedicated spike job. Do not extend
# spikes/ into real product code — once a spike's question is answered and
# recorded in the roadmap's Learnings log, the spike directory itself can
# be deleted.
option(MARKDAWN_BUILD_SPIKES "Build throwaway Phase 0.5 technical-spike executables" OFF)
if(MARKDAWN_BUILD_SPIKES)
    add_subdirectory(spikes/markdown-coverage)
endif()
