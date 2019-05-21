/* hyscan-cache.h
 *
 * Copyright 2015-2019 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanCache.
 *
 * HyScanCache is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCache is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCache имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCache на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

#ifndef __HYSCAN_CACHE_H__
#define __HYSCAN_CACHE_H__

#include <glib-object.h>
#include <hyscan-buffer.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CACHE            (hyscan_cache_get_type ())
#define HYSCAN_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (( obj ), HYSCAN_TYPE_CACHE, HyScanCache))
#define HYSCAN_IS_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (( obj ), HYSCAN_TYPE_CACHE))
#define HYSCAN_CACHE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE (( obj ), HYSCAN_TYPE_CACHE, HyScanCacheInterface))

typedef struct _HyScanCache HyScanCache;
typedef struct _HyScanCacheInterface HyScanCacheInterface;

/**
 * HyScanParamInterface:
 * @g_iface: Базовый интерфейс.
 * @set: Помещает данные в кэш.
 * @get: Считывает данные из кэша.
 */
struct _HyScanCacheInterface
{
  GTypeInterface g_iface;

  gboolean     (*set)                  (HyScanCache           *cache,
                                        guint64                key,
                                        guint64                detail,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

  gboolean     (*get)                  (HyScanCache           *cache,
                                        guint64                key,
                                        guint64                detail,
                                        guint32                size1,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);
};

HYSCAN_API
GType          hyscan_cache_get_type   (void);

HYSCAN_API
gboolean       hyscan_cache_set        (HyScanCache           *cache,
                                        const gchar           *key,
                                        const gchar           *detail,
                                        HyScanBuffer          *buffer);

HYSCAN_API
gboolean       hyscan_cache_set2       (HyScanCache           *cache,
                                        const gchar           *key,
                                        const gchar           *detail,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

HYSCAN_API
gboolean       hyscan_cache_set2i      (HyScanCache           *cache,
                                        guint64                key,
                                        guint64                detail,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

HYSCAN_API
gboolean       hyscan_cache_get        (HyScanCache           *cache,
                                        const gchar           *key,
                                        const gchar           *detail,
                                        HyScanBuffer          *buffer);

HYSCAN_API
gboolean       hyscan_cache_get2       (HyScanCache           *cache,
                                        const gchar           *key,
                                        const gchar           *detail,
                                        guint32                size1,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

HYSCAN_API
gboolean       hyscan_cache_get2i      (HyScanCache           *cache,
                                        guint64                key,
                                        guint64                detail,
                                        guint32                size1,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

G_END_DECLS

#endif /* __HYSCAN_CACHE_H__ */
