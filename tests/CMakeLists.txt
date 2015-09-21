
add_executable( cache-test cache-test.c )

target_link_libraries( cache-test ${GLIB2_LIBRARIES} hyscancache )
