# Self-tests. Plain assert-based executables + ctest (no gtest yet).
# ponytail: add gtest via vcpkg only when a test needs fixtures/parametrization.

add_executable(test_protocol test_protocol.cpp)
add_test(NAME protocol COMMAND test_protocol)

add_executable(test_ctrl test_ctrl.cpp)
target_link_libraries(test_ctrl PRIVATE idisplay_core)
add_test(NAME ctrl COMMAND test_ctrl)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE idisplay_core)
add_test(NAME config COMMAND test_config)
