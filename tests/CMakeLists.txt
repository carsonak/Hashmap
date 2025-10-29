include_directories(${PROJECT_SOURCE_DIR}/tau)
link_libraries(use_HashMap)

add_executable(test_HashMap test_HashMap.c)
add_test(NAME test_HashMap COMMAND test_HashMap --failed-output-only)

set_property(TEST test_HashMap PROPERTY TIMEOUT 5)
