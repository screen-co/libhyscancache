
add_library (hyscancache SHARED
             hyscan-cache.c
             hyscan-cached.c
             hyscan-cache-client.c
             hyscan-cache-server.c
             hyscan-hash.cc
             farmhash.cc)

target_link_libraries (hyscancache ${GLIB2_LIBRARIES} ${URPC_LIBRARIES})
