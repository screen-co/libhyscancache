
set (TEST_LIBRARIES ${GLIB2_LIBRARIES}
                    ${HYSCAN_LIBRARIES}
                    ${HYSCAN_CACHE_LIBRARY})

add_executable (cache-test cache-test.c)

target_link_libraries (cache-test ${TEST_LIBRARIES})

add_test (NAME CacheTest COMMAND cache-test -d 60 -m 256 -c -l -p 32 -t 2 -u -o 300000 -s 32 -b 1024
          WORKING_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

install (TARGETS cache-test
         COMPONENT test
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
