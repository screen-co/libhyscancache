
set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_CACHE_LIBRARY})

add_executable (cache-test cache-test.c)

target_link_libraries (cache-test ${TEST_LIBRARIES})

install (TARGETS cache-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
