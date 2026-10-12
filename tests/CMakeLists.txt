add_executable(test_buddy_coalesce test_buddy_coalesce.cpp)

target_link_libraries(test_buddy_coalesce PRIVATE slabcache)

add_test(NAME test_buddy_coalesce COMMAND test_buddy_coalesce)

add_executable(test_thread_cache test_thread_cache.cpp)

target_link_libraries(test_thread_cache PRIVATE slabcache)

add_test(NAME test_thread_cache COMMAND test_thread_cache)

