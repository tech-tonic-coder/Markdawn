find_package(Qt6 REQUIRED COMPONENTS Core Gui Network)
find_package(nlohmann_json REQUIRED)

# Phase 1: first real modules (document/, ipc/) replace the Phase 0
# placeholder (version.cpp) that only proved the two-target link worked.
#
# Headers with Q_OBJECT (document_model.h, single_instance_coordinator.h,
# and Phase 3's toc_model.h) are listed here alongside their .cpp files
# deliberately: CMAKE_AUTOMOC only moc's a header when it's part of the
# target's own SOURCES list (or shares a source's basename in the same
# directory) -- a header merely #included from a .cpp in a different
# directory is not enough, and silently produces an empty
# mocs_compilation.cpp (verified locally; see roadmap §6.2 Phase 1 log).
# heading_slug.h has no Q_OBJECT, so -- like ipc_message.h -- it does not
# need to be listed here; only its .cpp does.
add_library(markdawncore STATIC
    include/markdawncore/document/document_model.h
    src/document/document_model.cpp
    include/markdawncore/document/toc_model.h
    src/document/toc_model.cpp
    src/document/heading_slug.cpp
    include/markdawncore/ipc/single_instance_coordinator.h
    src/ipc/ipc_message.cpp
    src/ipc/single_instance_coordinator.cpp
)

target_include_directories(markdawncore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# PUBLIC: public headers under include/markdawncore/ (document_model.h,
# single_instance_coordinator.h) include Qt types directly, so consumers
# (markdawn-app) need these include paths/link flags too, not just this
# library's own .cpp files.
target_link_libraries(markdawncore
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Network
    PRIVATE
        nlohmann_json::nlohmann_json
        markdawn_warnings
)

set_target_properties(markdawncore PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
