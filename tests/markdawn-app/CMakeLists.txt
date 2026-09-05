# TabManager/DocumentView are QWidget subclasses, so this test needs a real
# QApplication (unlike tests/core-lib's plain QCoreApplication) -- run under
# the offscreen platform plugin.
#
# Compiles markdawn-app's tab_manager.cpp/document_view.cpp directly rather
# than linking against the `markdawn` executable target (linking against an
# executable as a dependency is fragile and not portable across platforms)
# or introducing a separate markdawn-app library target purely for this one
# test -- the smallest change that gets this specific regression covered.
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(tab_manager_test
    tab_manager_test.cpp
    ${CMAKE_SOURCE_DIR}/markdawn-app/src/tab_manager/tab_manager.h
    ${CMAKE_SOURCE_DIR}/markdawn-app/src/tab_manager/tab_manager.cpp
    ${CMAKE_SOURCE_DIR}/markdawn-app/src/document_view/document_view.h
    ${CMAKE_SOURCE_DIR}/markdawn-app/src/document_view/document_view.cpp
)

target_include_directories(tab_manager_test PRIVATE
    ${CMAKE_SOURCE_DIR}/markdawn-app/src
)

target_link_libraries(tab_manager_test PRIVATE
    Qt6::Widgets
    markdawncore
    markdawn_warnings
)

set_target_properties(tab_manager_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Same static-triplet plugin-linkage requirement as the real markdawn
# executable (markdawn-app/CMakeLists.txt's comment, roadmap §6.2 Phase 0
# log): on x64-linux/arm64-osx's static Qt triplet there is no loadable
# platform-plugin file at all, so each *executable* target needs its own
# qt_import_plugins() call -- a library dependency cannot provide this on
# an executable's behalf. Only the offscreen plugin is needed here (this
# test never opens a real window), unlike markdawn's own xcb/cocoa +
# offscreen pair.
if(NOT WIN32)
    qt_import_plugins(tab_manager_test INCLUDE
        Qt6::QOffscreenIntegrationPlugin
    )
endif()

# x64-windows is a *dynamic* triplet: the plugin is a separate DLL loaded at
# runtime from a "platforms" folder next to the exe (same mechanism as
# markdawn.exe's own post-build step, just copying the offscreen plugin
# instead of the windows one, since CI has no real display to target).
if(WIN32)
    add_custom_command(TARGET tab_manager_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:tab_manager_test>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:Qt6::QOffscreenIntegrationPlugin>"
                "$<TARGET_FILE_DIR:tab_manager_test>/platforms/"
        COMMENT "Copying the Qt offscreen platform plugin next to tab_manager_test"
    )
endif()

add_test(NAME tab_manager_test COMMAND tab_manager_test)
set_tests_properties(tab_manager_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
