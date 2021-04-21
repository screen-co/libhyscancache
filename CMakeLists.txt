
cmake_minimum_required (VERSION 3.0)

project (libhyscancache)

enable_testing()

set (HYSCAN_MAJOR_VERSION 5)
set (HYSCAN_CACHE_VERSION 0)
set (HYSCAN_CACHE_LIBRARY "hyscancache-${HYSCAN_MAJOR_VERSION}")

include (FindPkgConfig)
include (TestBigEndian)

if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE Release)
endif ()

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
  add_definitions (-DCPU32)
elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_definitions (-DCPU64)
else ()
  MATH (EXPR CPU_BITNESS 8*${CMAKE_SIZEOF_VOID_P})
  message (FATAL_ERROR "${CPU_BITNESS}bit is unsupported CPU bitness")
endif ()

test_big_endian (HYSCAN_CACHE_BIG_ENDIAN)
if (${HYSCAN_CACHE_BIG_ENDIAN})
  add_definitions (-DFARMHASH_BIG_ENDIAN=1)
else ()
  add_definitions (-DFARMHASH_LITTLE_ENDIAN=1)
endif ()

if (NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/bin")
  set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_SOURCE_DIR}/bin")
endif ()

if (${CMAKE_C_COMPILER_ID} STREQUAL GNU)
  set (DEBUG_OPT_LEVEL "-Og")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL Clang)
  set (DEBUG_OPT_LEVEL "-O1")
endif ()

if (${CMAKE_C_COMPILER_ID} STREQUAL GNU OR ${CMAKE_C_COMPILER_ID} STREQUAL Clang)
  set (C_FLAGS "-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 ${C_FLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${DEBUG_OPT_LEVEL} ${C_FLAGS}")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL MSVC)
  set (C_FLAGS "/wd4244 /wd4305 $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${C_FLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${C_FLAGS}")
  add_definitions (-DFARMHASH_OPTIONAL_BUILTIN_EXPECT)
else ()
  message (FATAL_ERROR "Unsupported compiler ${CMAKE_C_COMPILER_ID}")
endif ()

pkg_check_modules (GLIB2 REQUIRED glib-2.0 gobject-2.0 gthread-2.0 gio-2.0)
add_definitions (${GLIB2_CFLAGS})
link_directories (${GLIB2_LIBRARY_DIRS})

if (HYSCAN_SYS_LIBS)
  pkg_check_modules (HYSCAN REQUIRED "hyscantypes-${HYSCAN_MAJOR_VERSION}")
  pkg_check_modules (URPC REQUIRED urpc)

  link_directories (${HYSCAN_LIBRARY_DIRS} ${URPC_LIBRARY_DIRS})
  add_definitions (${HYSCAN_CFLAGS} ${URPC_CFLAGS})
else ()
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/hyscantypes")
  include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../urpc/urpc")

  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../libhyscantypes/bin")
  link_directories ("${CMAKE_CURRENT_SOURCE_DIR}/../urpc/bin")

  set (HYSCAN_LIBRARIES "hyscantypes-${HYSCAN_MAJOR_VERSION}")
  set (URPC_LIBRARIES urpc)
endif ()

include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscancache")

add_definitions (-DG_LOG_DOMAIN="HyScanCache")
add_subdirectory (hyscancache)
add_subdirectory (tests)
