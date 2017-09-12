
add_library (${HYSCAN_CACHE_LIBRARY} SHARED
             hyscan-cache.c
             hyscan-cached.c
             hyscan-cache-client.c
             hyscan-cache-server.c
             hyscan-hash.cc
             farmhash.cc)

target_link_libraries (${HYSCAN_CACHE_LIBRARY} ${GLIB2_LIBRARIES} ${URPC_LIBRARIES})

set_target_properties (${HYSCAN_CACHE_LIBRARY} PROPERTIES DEFINE_SYMBOL "HYSCAN_API_EXPORTS")
set_target_properties (${HYSCAN_CACHE_LIBRARY} PROPERTIES SOVERSION ${HYSCAN_CACHE_VERSION})

configure_file (hyscancache.pc.in
                "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscancache-${HYSCAN_MAJOR_VERSION}.pc"
                @ONLY)

install (TARGETS ${HYSCAN_CACHE_LIBRARY}
         COMPONENT runtime
         RUNTIME DESTINATION bin
         LIBRARY DESTINATION lib
         ARCHIVE DESTINATION lib
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/hyscancache-${HYSCAN_MAJOR_VERSION}.pc"
         COMPONENT development
         DESTINATION "lib/pkgconfig"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

install (FILES hyscan-cache.h
               hyscan-cached.h
               hyscan-cache-client.h
               hyscan-cache-server.h
         COMPONENT development
         DESTINATION "include/hyscan-${HYSCAN_MAJOR_VERSION}/hyscancache"
         PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
