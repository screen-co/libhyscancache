/*!
 * \file hyscan-cache.c
 *
 * \brief Исходный файл интерфейса системы кэширования HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
*/

#include "hyscan-cache.h"
#include "hyscan-hash.h"


G_DEFINE_INTERFACE( HyScanCache, hyscan_cache, G_TYPE_OBJECT );

static void hyscan_cache_default_init( HyScanCacheInterface *iface ){ ; }


gboolean hyscan_cache_set( HyScanCache *cache, const gchar *key, const gchar *detail, gpointer data, guint32 size )
{

  if( HYSCAN_CACHE_GET_IFACE( cache )->set )
    return HYSCAN_CACHE_GET_IFACE( cache )->set( cache, hyscan_hash64( key ), hyscan_hash64( detail ), data, size );

  return FALSE;

}


gboolean hyscan_cache_get( HyScanCache *cache, const gchar *key, const gchar *detail, gpointer buffer, guint32 *buffer_size )
{

  if( HYSCAN_CACHE_GET_IFACE( cache )->get )
    return HYSCAN_CACHE_GET_IFACE( cache )->get( cache, hyscan_hash64( key ), hyscan_hash64( detail ), buffer, buffer_size );

  return FALSE;

}
