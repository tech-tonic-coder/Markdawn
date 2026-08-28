# Phase 0.5, Spike A — throwaway executable, not part of the shipped app.
# Only built when MARKDAWN_BUILD_SPIKES=ON (see top-level CMakeLists.txt).

find_package(Qt6 REQUIRED COMPONENTS Gui)

add_executable(markdown-coverage-spike main.cpp)
target_link_libraries(markdown-coverage-spike PRIVATE Qt6::Gui markdawn_warnings)
