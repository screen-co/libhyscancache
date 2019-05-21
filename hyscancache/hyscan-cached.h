/* hyscan-cached.h
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

#ifndef __HYSCAN_CACHED_H__
#define __HYSCAN_CACHED_H__

#include <hyscan-cache.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CACHED             (hyscan_cached_get_type ())
#define HYSCAN_CACHED(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CACHED, HyScanCached))
#define HYSCAN_IS_CACHED(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CACHED))
#define HYSCAN_CACHED_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CACHED, HyScanCachedClass))
#define HYSCAN_IS_CACHED_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CACHED))
#define HYSCAN_CACHED_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CACHED, HyScanCachedClass))

typedef struct _HyScanCached HyScanCached;
typedef struct _HyScanCachedPrivate HyScanCachedPrivate;
typedef struct _HyScanCachedClass HyScanCachedClass;

struct _HyScanCached
{
  GObject parent_instance;

  HyScanCachedPrivate *priv;
};

struct _HyScanCachedClass
{
  GObjectClass parent_class;
};

HYSCAN_API
GType          hyscan_cached_get_type  (void);

HYSCAN_API
HyScanCached  *hyscan_cached_new       (guint32                cache_size);

G_END_DECLS

#endif /* __HYSCAN_CACHED_H__ */
