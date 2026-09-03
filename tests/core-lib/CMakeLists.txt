# First real test target in the project (§5 Phase 3's [Auto] "headless unit
# test, no GUI needed" requirement). Plain CTest + a small assert-and-count
# executable, not the QtTest framework: markdawncore already links Qt6::Core
# and Qt6::Gui PUBLIC (core-lib/CMakeLists.txt), which is everything this
# needs (QCoreApplication, QTextDocument), so linking against the
# markdawncore target is sufficient -- no new find_package/vcpkg feature
# required. See roadmap §6.2 Phase 3 log for why QtTest was considered and
# not used.
add_executable(toc_model_test toc_model_test.cpp)

target_link_libraries(toc_model_test PRIVATE
    markdawncore
    markdawn_warnings
)

set_target_properties(toc_model_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_test(NAME toc_model_test COMMAND toc_model_test)
