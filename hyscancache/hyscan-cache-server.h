/**
 * \file hyscan-cache-server.h
 *
 * \brief Заголовочный файл RPC сервера системы кэширования данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanCacheServer HyScanCacheServer - сервер системы кэширования данных
 *
 * Сервер системы кэширования данных представляет доступ к кэшу на основе реализации
 * \link HyScanCached \endlink по протоколу SHM библиотеки uRPC.
 *
 * Создать сервер системы кэширования можно с помощью функции #hyscan_cache_server_new.
 *
 */

#ifndef __HYSCAN_CACHE_SERVER_H__
#define __HYSCAN_CACHE_SERVER_H__

#include <glib-object.h>
#include <hyscan-cache-exports.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CACHE_SERVER             (hyscan_cache_server_get_type ())
#define HYSCAN_CACHE_SERVER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), HYSCAN_TYPE_CACHE_SERVER, HyScanCacheServer))
#define HYSCAN_IS_CACHE_SERVER(obj )         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), HYSCAN_TYPE_CACHE_SERVER))
#define HYSCAN_CACHE_SERVER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), HYSCAN_TYPE_CACHE_SERVER, HyScanCacheServerClass))
#define HYSCAN_IS_CACHE_SERVER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), HYSCAN_TYPE_CACHE_SERVER))
#define HYSCAN_CACHE_SERVER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), HYSCAN_TYPE_CACHE_SERVER, HyScanCacheServerClass))

typedef struct _HyScanCacheServer HyScanCacheServer;
typedef struct _HyScanCacheServerClass HyScanCacheServerClass;

struct _HyScanCacheServerClass
{
  GObjectClass parent_class;
};

HYSCAN_CACHE_EXPORT
GType                  hyscan_cache_server_get_type            (void);

/**
 *
 * Функция создаёт новый объект \link HyScanCacheServer \endlink.
 *
 * \param cache_uri путь к серверу кэширования;
 * \param cache_size объём памяти для кэша, Мб;
 * \param n_threads число потоков сервера;
 * \param n_clients максимальное число клиентов сервера.
 *
 * \return казатель на объект \link HyScanCacheServer \endlink.
 *
 */
HYSCAN_CACHE_EXPORT
HyScanCacheServer     *hyscan_cache_server_new                 (const gchar           *cache_uri,
                                                                guint32                cache_size,
                                                                guint32                n_threads,
                                                                guint32                n_clients);

G_END_DECLS

#endif /* __HYSCAN__H__ */
