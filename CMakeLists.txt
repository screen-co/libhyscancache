cmake_minimum_required (VERSION 2.6)

project (libhyscancache)

include (FindPkgConfig)
include (TestBigEndian)

#
# Default build type.
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE "Release")
endif ()

#
# CPU bitness.
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
  add_definitions (-DCPU32)
elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
  add_definitions (-DCPU64)
else ()
  MATH (EXPR CPU_BITNESS 8*${CMAKE_SIZEOF_VOID_P})
  message (FATAL_ERROR "${CPU_BITNESS}bit is unsupported CPU bitness")
endif ()

#
# Endianness check.
test_big_endian (HYSCAN_CACHE_BIG_ENDIAN)
if (${HYSCAN_CACHE_BIG_ENDIAN})
  add_definitions (-DFARMHASH_BIG_ENDIAN=1)
else ()
  add_definitions (-DFARMHASH_LITTLE_ENDIAN=1)
endif ()

#
# Compilers options.
if (${CMAKE_C_COMPILER_ID} STREQUAL "GNU" OR ${CMAKE_C_COMPILER_ID} STREQUAL "Clang")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -Wall $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -Wall $ENV{CFLAGS}")
elseif (${CMAKE_C_COMPILER_ID} STREQUAL "MSVC")
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /wd4244 /wd4305 $ENV{CFLAGS}")
  set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /wd4244 /wd4305 $ENV{CFLAGS}")
endif ()

#
# Output directories.
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

#
# System dependencies.
pkg_check_modules (GLIB2 REQUIRED "glib-2.0 gobject-2.0 gthread-2.0 gio-2.0")
add_definitions (${GLIB2_CFLAGS})
link_directories (${GLIB2_LIBRARY_DIRS})

#
# uRPC library default path.
if (NOT URPC_SOURCE_DIR)
  set (URPC_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../urpc)
endif ()

#
# uRPC dependencies.
pkg_check_modules (URPC QUIET urpc)
if (${URPC_FOUND})
  add_definitions (${URPC_CFLAGS})
  link_directories (${URPC_LIBRARY_DIRS})
else ()
  include_directories ("${URPC_SOURCE_DIR}/urpc")
  link_directories ("${URPC_SOURCE_DIR}/bin")
  set (URPC_LIBRARIES urpc)
endif ()

#
# Include directories.
include_directories ("${CMAKE_CURRENT_SOURCE_DIR}/hyscancache")

#
# Target.
add_definitions (-DG_LOG_DOMAIN="HyScanCache")
add_subdirectory ("hyscancache")
add_subdirectory ("tests")
