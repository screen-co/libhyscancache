/*
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
hyscan_cache_set  (HyScanCache * cache,
                   const gchar  *key,
                   const gchar  *detail,
                   HyScanBuffer *buffer)
{
  if (HYSCAN_CACHE_GET_IFACE (cache)->set != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->set (cache,
                                                  hyscan_hash64 (key),
                                                  hyscan_hash64 (detail),
                                                  buffer, NULL);
    }

  return FALSE;
}

gboolean
hyscan_cache_set2 (HyScanCache  *cache,
                   const gchar  *key,
                   const gchar  *detail,
                   HyScanBuffer *buffer1,
                   HyScanBuffer *buffer2)
{
  if (HYSCAN_CACHE_GET_IFACE (cache)->set != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->set (cache,
                                                  hyscan_hash64 (key),
                                                  hyscan_hash64 (detail),
                                                  buffer1, buffer2);
    }

  return FALSE;
}

gboolean
hyscan_cache_set2i (HyScanCache  *cache,
                    guint64       key,
                    guint64       detail,
                    HyScanBuffer *buffer1,
                    HyScanBuffer *buffer2)
{
  if (HYSCAN_CACHE_GET_IFACE (cache)->set != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->set (cache,
                                                  key, detail,
                                                  buffer1, buffer2);
    }

  return FALSE;
}

gboolean
hyscan_cache_get  (HyScanCache  *cache,
                   const gchar  *key,
                   const gchar  *detail,
                   HyScanBuffer *buffer)
{
  if( HYSCAN_CACHE_GET_IFACE (cache)->get != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->get (cache,
                                                  hyscan_hash64 (key),
                                                  hyscan_hash64 (detail),
                                                  G_MAXUINT32, buffer, NULL);
    }

  return FALSE;
}

gboolean
hyscan_cache_get2 (HyScanCache  *cache,
                   const gchar  *key,
                   const gchar  *detail,
                   guint32       size1,
                   HyScanBuffer *buffer1,
                   HyScanBuffer *buffer2)
{
  if( HYSCAN_CACHE_GET_IFACE (cache)->get != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->get (cache,
                                                  hyscan_hash64 (key),
                                                  hyscan_hash64 (detail),
                                                  size1, buffer1, buffer2);
    }

  return FALSE;
}

gboolean
hyscan_cache_get2i (HyScanCache  *cache,
                    guint64       key,
                    guint64       detail,
                    guint32       size1,
                    HyScanBuffer *buffer1,
                    HyScanBuffer *buffer2)
{
  if( HYSCAN_CACHE_GET_IFACE (cache)->get != NULL)
    {
     return HYSCAN_CACHE_GET_IFACE (cache)->get (cache,
                                                 key, detail,
                                                 size1, buffer1, buffer2);
    }

  return FALSE;
}
