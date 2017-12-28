/**
 * \file hyscan-cache-client.h
 *
 * \brief Заголовочный файл клиента системы кэширования данных на сервере
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanCacheClient HyScanCacheClient - реализация клиента системы кэширования данных на сервере
 *
 * HyScanCacheClient реализация интерфейса \link HyScanCache \endlink предназначенная для работы с сервером
 * кэширования \link HyScanCacheServer \endlink.
 *
 * Создать клиента системы кэширования можно с помощью функции #hyscan_cache_client_new.
 *
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
GType                  hyscan_cache_client_get_type    ( void );

/**
 *
 * Функция создаёт новый объект \link HyScanCacheClient \endlink.
 *
 * \param uri адрес сервера.
 *
 * \return Указатель на объект \link HyScanCacheClient \endlink.
 *
 */
HYSCAN_API
HyScanCacheClient     *hyscan_cache_client_new         (const gchar           *uri);

G_END_DECLS

#endif /* __HYSCAN_CACHE_CLIENT_H__ */
