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
 * кэширования \link HyScanCacheServer \endlink. Взаимодействие с сервером осуществляется с использованием
 * протокола SHM библиотеки uRPC.
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
typedef struct _HyScanCacheClientClass HyScanCacheClientClass;

struct _HyScanCacheClientClass
{
  GObjectClass parent_class;
};

HYSCAN_CACHE_EXPORT
GType hyscan_cache_client_get_type( void );

/**
 *
 * Функция создаёт новый объект \link HyScanCacheClient \endlink.
 *
 * \param cache_uri путь к серверу кэширования.
 *
 * \return Указатель на объект \link HyScanCacheClient \endlink.
 *
 */
HYSCAN_CACHE_EXPORT
HyScanCacheClient  *hyscan_cache_client_new            (gchar                 *cache_uri);

G_END_DECLS

#endif /* __HYSCAN_CACHE_CLIENT_H__ */
