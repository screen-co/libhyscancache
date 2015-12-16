/**
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

G_DEFINE_INTERFACE (HyScanCache, hyscan_cache, G_TYPE_OBJECT);

static void
hyscan_cache_default_init (HyScanCacheInterface *iface)
{
}

gboolean
hyscan_cache_set  (HyScanCache *cache,
                   const gchar *key,
                   const gchar *detail,
                   gpointer     data,
                   guint32      size)
{
  if (HYSCAN_CACHE_GET_IFACE (cache)->set != NULL)
    return HYSCAN_CACHE_GET_IFACE (cache)->set (cache, hyscan_hash64 (key), hyscan_hash64 (detail),
                                                       data, size);

  return FALSE;
}

gboolean
hyscan_cache_set2 (HyScanCache *cache,
                   const gchar *key,
                   const gchar *detail,
                   gpointer     data1,
                   guint32      size1,
                   gpointer     data2,
                   guint32      size2)
{
  if (HYSCAN_CACHE_GET_IFACE (cache)->set2 != NULL )
    return HYSCAN_CACHE_GET_IFACE (cache)->set2 (cache, hyscan_hash64 (key), hyscan_hash64 (detail),
                                                        data1, size1, data2, size2);

  return FALSE;
}

gboolean
hyscan_cache_get  (HyScanCache *cache,
                   const gchar *key,
                   const gchar *detail,
                   gpointer     buffer,
                   guint32     *buffer_size)
{
  if( HYSCAN_CACHE_GET_IFACE (cache)->get != NULL )
    return HYSCAN_CACHE_GET_IFACE (cache)->get (cache, hyscan_hash64 (key), hyscan_hash64 (detail),
                                                       buffer, buffer_size);

  return FALSE;
}

gboolean
hyscan_cache_get2 (HyScanCache *cache,
                   const gchar *key,
                   const gchar *detail,
                   gpointer     buffer1,
                   guint32     *buffer1_size,
                   gpointer     buffer2,
                   guint32     *buffer2_size)
{
  if( HYSCAN_CACHE_GET_IFACE (cache)->get2 != NULL )
    return HYSCAN_CACHE_GET_IFACE (cache)->get2 (cache, hyscan_hash64 (key), hyscan_hash64 (detail),
                                                        buffer1, buffer1_size, buffer2, buffer2_size);

  return FALSE;
}
