/* hyscan-cache-server.c
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

/**
 * SECTION: hyscan-cache-server
 * @Short_description: сервера системы кэширования данных
 * @Title: HyScanCacheServer
 *
 * Сервер кеша данных транслирует все вызовы интерфейса #HyScanCache в объект
 * cache, указанный при создании сервера.
 *
 * Создать сервер системы кэширования можно с помощью функции
 * #hyscan_cache_server_new.
 *
 * После создания сервера его необходимо запустить функцией
 * #hyscan_cache_server_start.
 */

#include "hyscan-cache-server.h"
#include "hyscan-cache-rpc.h"
#include "hyscan-cached.h"

#include <urpc-server.h>

#define hyscan_cache_server_set_error(p)   do { \
                                             g_warning ("%s: can't set '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_cache_server_get_error(p)   do { \
                                             g_warning ("%s: can't get '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

enum
{
  PROP_O,
  PROP_URI,
  PROP_CACHE,
  PROP_N_THREADS,
  PROP_N_CLIENTS
};

struct _HyScanCacheServerPrivate
{
  volatile gint        running;                /* Признак запуска сервера. */

  uRpcServer          *rpc;                    /* RPC сервер. */
  gchar               *uri;                    /* Путь к RPC серверу. */
  HyScanCache         *cache;                  /* Кэш данных. */

  guint32              n_threads;              /* Число RPC потоков. */
  guint32              n_clients;              /* Максимальное число клиентов. */
};

static void    hyscan_cache_server_set_property        (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_cache_server_object_finalize     (GObject               *object);

static gint    hyscan_cache_server_rpc_proc_version    (uRpcData              *urpc_data,
                                                        void                  *thread_data,
                                                        void                  *session_data,
                                                        void                  *proc_data);
static gint    hyscan_cache_server_rpc_proc_set        (uRpcData              *urpc_data,
                                                        void                  *thread_data,
                                                        void                  *session_data,
                                                        void                  *proc_data);
static gint    hyscan_cache_server_rpc_proc_get        (uRpcData              *urpc_data,
                                                        void                  *thread_data,
                                                        void                  *session_data,
                                                        void                  *proc_data);

G_DEFINE_TYPE_WITH_PRIVATE (HyScanCacheServer, hyscan_cache_server, G_TYPE_OBJECT);

static void
hyscan_cache_server_class_init (HyScanCacheServerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->set_property = hyscan_cache_server_set_property;
  object_class->finalize = hyscan_cache_server_object_finalize;

  g_object_class_install_property (object_class, PROP_URI,
                                   g_param_spec_string ("uri", "Uri", "HyScan cache uri", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_CACHE,
                                   g_param_spec_object ("cache", "Cache", "HyScan cache", HYSCAN_TYPE_CACHE,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_THREADS,
                                   g_param_spec_uint ("n-threads", "Number of threads", "Number of threads",
                                                      1, URPC_MAX_THREADS_NUM, 1,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_N_CLIENTS,
                                   g_param_spec_uint ("n-clients", "Number of clients", "Maximum number of clients",
                                                      1, 1000, 1000,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_cache_server_init (HyScanCacheServer *server)
{
  server->priv = hyscan_cache_server_get_instance_private (server);
}

static void
hyscan_cache_server_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanCacheServer *server = HYSCAN_CACHE_SERVER (object);
  HyScanCacheServerPrivate *priv = server->priv;

  switch (prop_id)
    {
    case PROP_URI:
      priv->uri = g_value_dup_string (value);
      break;

    case PROP_CACHE:
      priv->cache = g_value_dup_object (value);
      break;

    case PROP_N_THREADS:
      priv->n_threads = g_value_get_uint (value);
      break;

    case PROP_N_CLIENTS:
      priv->n_clients = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_cache_server_object_finalize (GObject *object)
{
  HyScanCacheServer *server = HYSCAN_CACHE_SERVER (object);
  HyScanCacheServerPrivate *priv = server->priv;

  if (priv->rpc != NULL)
    urpc_server_destroy (priv->rpc);

  if (priv->cache != NULL)
    g_object_unref (priv->cache);

  g_free (priv->uri);

  G_OBJECT_CLASS (hyscan_cache_server_parent_class)->finalize (object);
}

/* Функция создаёт буфер данных потока исполнения RPC. */
static void *
hyscan_cache_server_rpc_thread_start (gpointer user_data)
{
  return hyscan_buffer_new ();
}

/* Функция удаляет буфер данных потока исполнения RPC. */
static void
hyscan_cache_server_rpc_thread_stop (gpointer thread_data,
                                     gpointer user_data)
{
  g_object_unref (thread_data);
}

/* RPC функция HYSCAN_CACHE_RPC_PROC_VERSION. */
static gint
hyscan_cache_server_rpc_proc_version (uRpcData *urpc_data,
                                      void     *thread_data,
                                      void     *session_data,
                                      void     *proc_data)
{
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_VERSION, HYSCAN_CACHE_RPC_VERSION);

  return 0;
}

/* RPC функция HYSCAN_CACHE_RPC_PROC_SET. */
static gint
hyscan_cache_server_rpc_proc_set (uRpcData *urpc_data,
                                  void     *thread_data,
                                  void     *session_data,
                                  void     *proc_data)
{
  HyScanCacheServerPrivate *priv = proc_data;

  guint32 rpc_status = HYSCAN_CACHE_RPC_STATUS_FAIL;
  gboolean status = FALSE;

  HyScanBuffer *buffer = NULL;

  guint64  key;
  guint64  detail;
  gpointer data;
  guint32  size;

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY, &key) != 0)
    hyscan_cache_server_get_error ("key");

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL, &detail) != 0)
    detail = 0;

  data = urpc_data_get (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA, &size);
  if (data != NULL)
    {
      buffer = thread_data;
      hyscan_buffer_wrap (buffer, HYSCAN_DATA_BLOB, data, size);
    }

  status = hyscan_cache_set2i (priv->cache, key, detail, buffer, NULL);
  if (status)
    rpc_status = HYSCAN_CACHE_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/* RPC функция HYSCAN_CACHE_RPC_PROC_GET. */
static gint
hyscan_cache_server_rpc_proc_get (uRpcData *urpc_data,
                                  void     *thread_data,
                                  void     *session_data,
                                  void     *proc_data)
{
  HyScanCacheServerPrivate *priv = proc_data;

  guint32 rpc_status = HYSCAN_CACHE_RPC_STATUS_FAIL;
  HyScanBuffer *buffer = thread_data;

  guint64  key;
  guint64  detail;
  gpointer data;
  guint32  size;
  gboolean status = FALSE;

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY, &key) != 0)
    hyscan_cache_server_get_error ("key");

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL, &detail) != 0)
    detail = 0;

  size = URPC_MAX_DATA_SIZE - 1024;
  data = urpc_data_set (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA, NULL, size);
  if (data == NULL)
    hyscan_cache_server_set_error ("data");

  hyscan_buffer_wrap (buffer, HYSCAN_DATA_BLOB, data, size);

  status = hyscan_cache_get2i (priv->cache, key, detail, G_MAXUINT32, buffer, NULL);
  if (status)
    {
      if (hyscan_buffer_get (buffer, NULL, &size) == NULL)
        size = 0;

      if (urpc_data_set (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA, NULL, size) == NULL )
        hyscan_cache_server_set_error ("data-size");

      rpc_status = HYSCAN_CACHE_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

/**
 * hyscan_cache_server_new:
 * @uri: адрес сервера
 * @cache: объект в который транслируются запросы клиентов
 * @n_threads: число потоков сервера
 * @n_clients: максимальное число клиентов сервера
 *
 * Функция создаёт новый объект #HyScanCacheServer.
 *
 * Returns: #HyScanCacheServer. Для удаления #g_object_unref.
 */
HyScanCacheServer *
hyscan_cache_server_new (const gchar *uri,
                         HyScanCache *cache,
                         guint32      n_threads,
                         guint32      n_clients)
{
  return g_object_new (HYSCAN_TYPE_CACHE_SERVER,
                       "uri", uri,
                       "cache", cache,
                       "n-threads", n_threads,
                       "n-clients", n_clients,
                       NULL);
}

/**
 * hyscan_cache_server_start:
 * @server: указатель на #HyScanCacheServer
 *
 * Функция запускает сервер системы кэширования в работу.
 *
 * Returns: %TRUE если сервер запущен, иначе %FALSE.
 */
gboolean
hyscan_cache_server_start (HyScanCacheServer *server)
{
  HyScanCacheServerPrivate *priv;

  uRpcType rpc_type;
  gint status;

  g_return_val_if_fail (HYSCAN_IS_CACHE_SERVER (server), FALSE);

  priv = server->priv;

  /* Проверяем что сервер ещё не запущен. */
  if (g_atomic_int_get (&priv->running))
    return FALSE;

  /* Проверяем адрес сервера. */
  if (priv->uri == NULL)
    return FALSE;

  /* Проверяем тип RPC протокола. */
  rpc_type = urpc_get_type (priv->uri);
  if (rpc_type != URPC_TCP && rpc_type != URPC_SHM)
    return FALSE;

  /* Проверяем кэш данных HyScan. */
  if (priv->cache == NULL)
    return FALSE;

  /* Создаём RPC сервер. */
  priv->rpc = urpc_server_create (priv->uri, priv->n_threads, priv->n_clients,
                                  URPC_DEFAULT_SESSION_TIMEOUT,
                                  URPC_MAX_DATA_SIZE,
                                  URPC_DEFAULT_DATA_TIMEOUT);
  if (priv->rpc == NULL)
    return FALSE;

  /* Функции инициализации потоков исполнения. */
  status = urpc_server_add_thread_start_callback (priv->rpc,
                                                  hyscan_cache_server_rpc_thread_start,
                                                  NULL);
  if (status != 0)
    goto fail;

  status = urpc_server_add_thread_stop_callback (priv->rpc,
                                                 hyscan_cache_server_rpc_thread_stop,
                                                 NULL);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_CACHE_RPC_PROC_VERSION,
                                     hyscan_cache_server_rpc_proc_version, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_CACHE_RPC_PROC_SET,
                                     hyscan_cache_server_rpc_proc_set, priv);
  if (status != 0)
    goto fail;

  status = urpc_server_add_callback (priv->rpc, HYSCAN_CACHE_RPC_PROC_GET,
                                     hyscan_cache_server_rpc_proc_get, priv);
  if (status != 0)
    goto fail;

  /* Запуск RPC сервера. */
  status = urpc_server_bind (priv->rpc);
  if (status != 0)
    goto fail;

  g_atomic_int_set (&priv->running, 1);
  return TRUE;

fail:
  urpc_server_destroy (priv->rpc);
  priv->rpc = NULL;
  return FALSE;
}
