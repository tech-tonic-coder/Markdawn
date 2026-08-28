# Phase 0: placeholder library so the two-target architecture (§2.1) exists
# and links correctly before any real module (document/, ipc/, settings/,
# highlighting/, logging/) has content. src/version.cpp is removed once
# Phase 1 adds the first real source file.
add_library(markdawncore STATIC
    src/version.cpp
)

target_include_directories(markdawncore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(markdawncore PRIVATE markdawn_warnings)

set_target_properties(markdawncore PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
