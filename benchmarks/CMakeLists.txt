# This macro defines 4 executables for a given base configuration
macro(add_hashmap_variants type base_name base_defines)
    set(source_file "${type}_HashMap.c")

    set(target_1 "${type}_${base_name}")
    add_executable(${target_1} ${source_file})
    target_compile_options(${target_1} PRIVATE ${base_defines})
    target_link_libraries(${target_1} PRIVATE use_HashMap)

    set(target_2 "${type}_${base_name}_stack")
    add_executable(${target_2} ${source_file})
    target_compile_options(${target_2} PRIVATE ${base_defines} -DEMPTY_BUCKET_STACK)
    target_link_libraries(${target_2} PRIVATE use_HashMap)

    set(target_3 "${type}_${base_name}_cellar")
    add_executable(${target_3} ${source_file})
    target_compile_options(${target_3} PRIVATE ${base_defines} -DCELLAR_COALESCED_HASHING)
    target_link_libraries(${target_3} PRIVATE use_HashMap)

    set(target_4 "${type}_${base_name}_cellar_stack")
    add_executable(${target_4} ${source_file})
    target_compile_options(${target_4} PRIVATE ${base_defines} -DCELLAR_COALESCED_HASHING -DEMPTY_BUCKET_STACK)
    target_link_libraries(${target_4} PRIVATE use_HashMap)
endmacro()

# Loop over the 2 base files and their 4 base configs
foreach(exec_type IN ITEMS benchmark time)
    # --- Murmur3Hash Configurations ---
    set(murmur3_base_defines "-DMURMURHASH3_x86_32_FUNC")
    add_hashmap_variants(${exec_type} "murmur3" "${murmur3_base_defines}")

    set(murmur3_power2_defines "${murmur3_base_defines} -DPOWER2_ROUNDUP_FUNC")
    add_hashmap_variants(${exec_type} "murmur3_power2" "${murmur3_power2_defines}")

    # --- FNV-1a Configurations ---
    set(fnv1a_base_defines "-DFNV32A_HASH_FUNC")
    add_hashmap_variants(${exec_type} "fnv1a" "${fnv1a_base_defines}")

    set(fnv1a_power2_defines "${fnv1a_base_defines} -DPOWER2_ROUNDUP_FUNC")
    add_hashmap_variants(${exec_type} "fnv1a_power2" "${fnv1a_power2_defines}")
endforeach()
