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
 * Сервер кеша данных транслирует все вызовы интерфейса \link HyScanCache \endlink в
 * объект cache, указанный при создании сервера.
 *
 * Создать сервер системы кэширования можно с помощью функции #hyscan_cache_server_new.
 *
 * После создания сервера его необходимо запустить функцией #hyscan_cache_server_start.
 *
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

/**
 *
 * Функция создаёт новый объект \link HyScanCacheServer \endlink.
 *
 * \param uri адрес сервера;
 * \param cache объект в который транслируются запросы клиентов;
 * \param n_threads число потоков сервера;
 * \param n_clients максимальное число клиентов сервера.
 *
 * \return казатель на объект \link HyScanCacheServer \endlink.
 *
 */
HYSCAN_API
HyScanCacheServer     *hyscan_cache_server_new         (const gchar           *uri,
                                                        HyScanCache           *cache,
                                                        guint32                n_threads,
                                                        guint32                n_clients);

/**
 *
 * Функция запускает сервер системы кэширования в работу.
 *
 * \param server указатель на объект \link HyScanCacheServer \endlink.
 *
 * \return TRUE - если сервер успешно запущен, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean               hyscan_cache_server_start       (HyScanCacheServer     *server);

G_END_DECLS

#endif /* __HYSCAN__H__ */
