target_include_directories(HashMap INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_sources(use_HashMap PRIVATE u8mem.c)
target_link_directories(use_HashMap PUBLIC ${CMAKE_CURRENT_LIST_DIR})
