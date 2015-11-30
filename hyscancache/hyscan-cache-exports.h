#ifndef __HYSCAN_CACHE_EXPORTS_H__
#define __HYSCAN_CACHE_EXPORTS_H__

#if defined (_WIN32)
  #if defined (hyscancache_EXPORTS)
    #define HYSCAN_CACHE_EXPORT __declspec (dllexport)
  #else
    #define HYSCAN_CACHE_EXPORT __declspec (dllimport)
  #endif
#else
  #define HYSCAN_CACHE_EXPORT
#endif

#endif /* __HYSCAN_CACHE_EXPORTS_H__ */
