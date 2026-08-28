find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(markdawn
    src/main.cpp
)

target_link_libraries(markdawn PRIVATE
    Qt6::Widgets
    markdawncore
    markdawn_warnings
)

set_target_properties(markdawn PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Qt cannot create any window without its platform-integration plugin
# (qwindows/qxcb/qcocoa) — without it, QApplication's constructor calls
# qFatal("Could not find the Qt platform plugin..."), which aborts via
# qAbort(). A plain build does not make that plugin reachable at runtime by
# itself; how to fix it depends on the triplet's library linkage
# (build/vcpkg_installed/<triplet>/... vs CMakePresets.json's
# VCPKG_TARGET_TRIPLET — see roadmap §6.2, Phase 0 log for the verified
# per-triplet linkage):
#   - x64-windows is a *dynamic* triplet: the plugin is a separate DLL Qt
#     loads at runtime from a "platforms" folder next to the exe. Copy it
#     there after every build.
#   - x64-linux and arm64-osx are *static* triplets: there is no loadable
#     plugin file at all — its object code must be linked directly into the
#     executable via Qt's own qt_import_plugins() CMake macro.
if(WIN32)
    add_custom_command(TARGET markdawn POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:markdawn>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                "$<TARGET_FILE_DIR:markdawn>/platforms/"
        COMMENT "Copying the Qt Windows platform plugin next to markdawn.exe"
    )
elseif(APPLE)
    qt_import_plugins(markdawn INCLUDE
        Qt6::QCocoaIntegrationPlugin
        Qt6::QOffscreenIntegrationPlugin
    )
else()
    # xcb for a real desktop session; offscreen so the Linux CI smoke test's
    # QT_QPA_PLATFORM=offscreen (§5 Phase 0) keeps working too.
    qt_import_plugins(markdawn INCLUDE
        Qt6::QXcbIntegrationPlugin
        Qt6::QOffscreenIntegrationPlugin
    )
endif()

# WIN32_EXECUTABLE (no console window) is deliberately not set yet — Phase 0
# wants console/qDebug output visible while bringing the app up. Revisit in
# Phase 12 packaging once logging (§4.11) is the primary diagnostic channel.
