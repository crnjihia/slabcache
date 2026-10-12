add_executable(test_buddy_coalesce test_buddy_coalesce.cpp)

target_link_libraries(test_buddy_coalesce PRIVATE slabcache)

add_test(NAME test_buddy_coalesce COMMAND test_buddy_coalesce)

add_executable(test_thread_cache test_thread_cache.cpp)

target_link_libraries(test_thread_cache PRIVATE slabcache)

add_executable(test_frag_metrics test_frag_metrics.cpp)

target_link_libraries(test_frag_metrics PRIVATE slabcache)

add_executable(test_hash_robin_hood test_hash_robin_hood.cpp)

target_link_libraries(test_hash_robin_hood PRIVATE slabcache)

add_executable(test_lru_eviction test_lru_eviction.cpp)

target_link_libraries(test_lru_eviction PRIVATE slabcache)

add_executable(test_ttl_expiry test_ttl_expiry.cpp)

target_link_libraries(test_ttl_expiry PRIVATE slabcache)

add_test(NAME test_ttl_expiry COMMAND test_ttl_expiry)


