target_include_directories(HashMap INTERFACE ${CMAKE_CURRENT_LIST_DIR})

target_sources(use_HashMap PRIVATE xalloc.c)
target_link_directories(use_HashMap PUBLIC ${CMAKE_CURRENT_LIST_DIR})
