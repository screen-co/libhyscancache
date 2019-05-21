/* hyscan-cache-client.h
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

#ifndef __HYSCAN_CACHE_CLIENT_H__
#define __HYSCAN_CACHE_CLIENT_H__

#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CACHE_CLIENT             (hyscan_cache_client_get_type ())
#define HYSCAN_CACHE_CLIENT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CACHE_CLIENT, HyScanCacheClient))
#define HYSCAN_IS_CACHE_CLIENT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CACHE_CLIENT))
#define HYSCAN_CACHE_CLIENT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CACHE_CLIENT, HyScanCacheClientClass))
#define HYSCAN_IS_CACHE_CLIENT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CACHE_CLIENT))
#define HYSCAN_CACHE_CLIENT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CACHE_CLIENT, HyScanCacheClientClass))

typedef struct _HyScanCacheClient HyScanCacheClient;
typedef struct _HyScanCacheClientPrivate HyScanCacheClientPrivate;
typedef struct _HyScanCacheClientClass HyScanCacheClientClass;

struct _HyScanCacheClient
{
  GObject parent_instance;

  HyScanCacheClientPrivate *priv;
};

struct _HyScanCacheClientClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_cache_client_get_type    (void);

HYSCAN_API
HyScanCacheClient     *hyscan_cache_client_new         (const gchar           *uri);

G_END_DECLS

#endif /* __HYSCAN_CACHE_CLIENT_H__ */
