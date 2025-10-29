cmake_minimum_required(VERSION 3.27)

project(
    HashMap
    DESCRIPTION "Simple arbitrary precision calculator"
    LANGUAGES C
)

include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_available)
if(LTO_available)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION
        "$<NOT:$<CONFIG:Debug>>"
        CACHE BOOL "Enable Interprocedural Optimisations for non-debug builds."
    )
endif()

# Compiler warnings
set(CMAKE_C_STANDARD 17)
add_compile_options(
    -Wall
    -Wextra
    -pedantic
    -Wformat=2
    -Wshadow
    "$<$<C_COMPILER_ID:GNU>:-Wduplicated-branches>"
    "$<$<C_COMPILER_ID:GNU>:-Wduplicated-cond>"
    "$<$<C_COMPILER_ID:Clang>:-Wno-gcc-compat>"
    -Werror
    "$<$<CONFIG:Debug,RelWithDebInfo>:-fsanitize=address,undefined>"
    "$<$<AND:$<C_COMPILER_ID:GNU,Clang>,$<CONFIG:Debug>>:-Og>"
)
add_link_options(
    "$<$<CONFIG:Debug,RelWithDebInfo>:-fsanitize=address,undefined>"
)

add_library(HashMap INTERFACE)
target_include_directories(HashMap INTERFACE ${CMAKE_CURRENT_LIST_DIR})

add_library(use_HashMap HashMap.c)
target_link_libraries(use_HashMap HashMap)
set_source_files_properties(
    HashMap.c PROPERTIES COMPILE_OPTIONS "-Wno-implicit-fallthrough"
)

add_subdirectory(benchmarks)
add_subdirectory(xalloc)
add_subdirectory(u8mem)

# Tests are automatically built if this is the main project
option(HASHMAP_ENABLE_TESTING "Enable tests for bigint_calc"
       ${PROJECT_IS_TOP_LEVEL}
)

if(HASHMAP_ENABLE_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
