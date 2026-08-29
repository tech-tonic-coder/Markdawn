# Phase 0.5, Spike A — throwaway executable, not part of the shipped app.
# Only built when MARKDAWN_BUILD_SPIKES=ON (see top-level CMakeLists.txt).

find_package(Qt6 REQUIRED COMPONENTS Gui)

add_executable(markdown-coverage-spike main.cpp)
target_link_libraries(markdown-coverage-spike PRIVATE Qt6::Gui markdawn_warnings)

# Same static-vs-dynamic-triplet plugin problem as markdawn-app (see
# markdawn-app/CMakeLists.txt's comment for the full explanation) — without
# this, the offscreen platform plugin isn't reachable at runtime and the
# executable qFatal()/aborts before printing anything (confirmed via CI:
# exit code 134 on Linux, see roadmap §6.2 Phase 0.5 log).
if(WIN32)
    add_custom_command(TARGET markdown-coverage-spike POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:markdown-coverage-spike>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:markdown-coverage-spike>/platforms/"
        COMMENT "Copying the Qt Windows platform plugin next to markdown-coverage-spike.exe"
    )
elseif(APPLE)
    qt_import_plugins(markdown-coverage-spike INCLUDE
        Qt6::QOffscreenIntegrationPlugin
    )
else()
    qt_import_plugins(markdown-coverage-spike INCLUDE
        Qt6::QOffscreenIntegrationPlugin
    )
endif()
