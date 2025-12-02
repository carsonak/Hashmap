include_directories(${PROJECT_SOURCE_DIR}/tau)

# set(HASHMAP_COMMON_SOURCES ${HASHMAP_COMMON_SOURCES} HashMap.c)

# add_executable(test_HashMap test_HashMap.c "${HASHMAP_COMMON_SOURCES}")
add_executable(test_HashMap test_HashMap.c HashMap.c)
target_link_libraries(test_HashMap PRIVATE HashMap)
add_test(NAME test_HashMap COMMAND test_HashMap --failed-output-only)

set_property(TEST test_HashMap PROPERTY TIMEOUT 5)
