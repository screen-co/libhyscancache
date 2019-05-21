/* hyscan-cache-server.h
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

#ifndef __HYSCAN_CACHE_SERVER_H__
#define __HYSCAN_CACHE_SERVER_H__

#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CACHE_SERVER             (hyscan_cache_server_get_type ())
#define HYSCAN_CACHE_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CACHE_SERVER, HyScanCacheServer))
#define HYSCAN_IS_CACHE_SERVER(obj )         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CACHE_SERVER))
#define HYSCAN_CACHE_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CACHE_SERVER, HyScanCacheServerClass))
#define HYSCAN_IS_CACHE_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CACHE_SERVER))
#define HYSCAN_CACHE_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CACHE_SERVER, HyScanCacheServerClass))

typedef struct _HyScanCacheServer HyScanCacheServer;
typedef struct _HyScanCacheServerPrivate HyScanCacheServerPrivate;
typedef struct _HyScanCacheServerClass HyScanCacheServerClass;

struct _HyScanCacheServer
{
  GObject parent_instance;

  HyScanCacheServerPrivate *priv;
};

struct _HyScanCacheServerClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType                  hyscan_cache_server_get_type    (void);

HYSCAN_API
HyScanCacheServer     *hyscan_cache_server_new         (const gchar           *uri,
                                                        HyScanCache           *cache,
                                                        guint32                n_threads,
                                                        guint32                n_clients);

HYSCAN_API
gboolean               hyscan_cache_server_start       (HyScanCacheServer     *server);

G_END_DECLS

#endif /* __HYSCAN__H__ */
