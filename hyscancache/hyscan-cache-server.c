/**
 * \file hyscan-cache-server.c
 *
 * \brief Исходный файл RPC сервера системы кэширования данных
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-cache-server.h"
#include "hyscan-cache-rpc.h"
#include "hyscan-cached.h"

#include <urpc-server.h>

enum
{
  PROP_O,
  PROP_URI,
  PROP_SIZE,
  PROP_N_THREADS,
  PROP_N_CLIENTS
};

struct _HyScanCacheServer
{
  GObject              parent_instance;

  gchar               *uri;
  guint32              size;
  HyScanCache         *cache;

  guint32              n_threads;
  guint32              n_clients;
  uRpcServer          *rpc;
};

static void    hyscan_cache_server_set_property        (GObject               *object,
                                                        guint                  prop_id,
                                                        const GValue          *value,
                                                        GParamSpec            *pspec);
static void    hyscan_cache_server_object_constructed  (GObject               *object);
static void    hyscan_cache_server_object_finalize     (GObject               *object);

static gint    hyscan_cache_server_rpc_proc_version    (guint32                session,
                                                        uRpcData              *urpc_data,
                                                        void                  *proc_data,
                                                        void                  *key_data);
static gint    hyscan_cache_server_rpc_proc_set        (guint32                session,
                                                        uRpcData              *urpc_data,
                                                        void                  *proc_data,
                                                        void                  *key_data);
static gint    hyscan_cache_server_rpc_proc_get        (guint32                session,
                                                        uRpcData              *urpc_data,
                                                        void                  *proc_data,
                                                        void                  *key_data);
static gint    hyscan_cache_server_rpc_proc_size       (guint32                session,
                                                        uRpcData              *urpc_data,
                                                        void                  *proc_data,
                                                        void                  *key_data);

G_DEFINE_TYPE (HyScanCacheServer, hyscan_cache_server, G_TYPE_OBJECT);

static void hyscan_cache_server_class_init( HyScanCacheServerClass *klass )
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->set_property = hyscan_cache_server_set_property;
  object_class->constructed = hyscan_cache_server_object_constructed;
  object_class->finalize = hyscan_cache_server_object_finalize;

  g_object_class_install_property (object_class, PROP_URI,
                                   g_param_spec_string ("uri", "Uri", "Cache uri", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (object_class, PROP_SIZE,
                                   g_param_spec_uint ("size", "Size", "Cache size, Mb",
                                                      0, G_MAXUINT32, 256,
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
hyscan_cache_server_init (HyScanCacheServer *cache_server)
{
}

static void
hyscan_cache_server_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  HyScanCacheServer *cache_server = HYSCAN_CACHE_SERVER (object);

  switch (prop_id)
    {
    case PROP_URI:
      cache_server->uri = g_strdup_printf ("shm://%s", g_value_get_string (value));
      break;

    case PROP_SIZE:
      cache_server->size = g_value_get_uint (value);
      break;

    case PROP_N_THREADS:
      cache_server->n_threads = g_value_get_uint (value);
      break;

    case PROP_N_CLIENTS:
      cache_server->n_clients = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_cache_server_object_constructed (GObject *object)
{
  HyScanCacheServer *cache_server = HYSCAN_CACHE_SERVER (object);
  gint status;

  cache_server->cache = HYSCAN_CACHE (hyscan_cached_new (cache_server->size));

  cache_server->rpc = urpc_server_create (cache_server->uri, cache_server->n_threads, cache_server->n_clients,
                                          URPC_DEFAULT_SESSION_TIMEOUT, URPC_MAX_DATA_SIZE, URPC_DEFAULT_DATA_TIMEOUT);
  if (cache_server->rpc == NULL)
    return;

  status = urpc_server_add_proc (cache_server->rpc, HYSCAN_CACHE_RPC_PROC_VERSION,
                                 hyscan_cache_server_rpc_proc_version, cache_server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (cache_server->rpc, HYSCAN_CACHE_RPC_PROC_SET,
                                 hyscan_cache_server_rpc_proc_set, cache_server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (cache_server->rpc, HYSCAN_CACHE_RPC_PROC_GET,
                                 hyscan_cache_server_rpc_proc_get, cache_server);
  if (status != 0)
    goto fail;

  status = urpc_server_add_proc (cache_server->rpc, HYSCAN_CACHE_RPC_PROC_SIZE,
                                 hyscan_cache_server_rpc_proc_size, cache_server);
  if (status != 0)
    goto fail;

  status = urpc_server_bind (cache_server->rpc);
  if (status != 0)
    goto fail;

  return;

fail:
  urpc_server_destroy (cache_server->rpc);
  cache_server->rpc = NULL;
}

static void
hyscan_cache_server_object_finalize (GObject *object)
{
  HyScanCacheServer *cache_server = HYSCAN_CACHE_SERVER (object);

  g_free (cache_server->uri);

  if (cache_server->rpc != NULL)
    urpc_server_destroy (cache_server->rpc);

  g_object_unref (cache_server->cache);

  G_OBJECT_CLASS (hyscan_cache_server_parent_class)->finalize (object);
}

/* RPC функция HYSCAN_CACHE_RPC_PROC_VERSION. */
static gint
hyscan_cache_server_rpc_proc_version (guint32   session,
                                      uRpcData *urpc_data,
                                      void     *proc_data,
                                      void     *key_data)
{
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_VERSION, HYSCAN_CACHE_RPC_VERSION);

  return 0;
}

/* RPC функция HYSCAN_CACHE_RPC_PROC_SET. */
static gint
hyscan_cache_server_rpc_proc_set (guint32   session,
                                  uRpcData *urpc_data,
                                  void     *proc_data,
                                  void     *key_data)
{
  HyScanCacheServer *cache_server = proc_data;

  guint64  key;
  guint64  detail;
  gpointer data1;
  guint32  size1;
  gpointer data2;
  guint32  size2;
  gboolean status = FALSE;

  key = urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY);
  detail = urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL);
  if (key == 0)
    goto exit;

  data1 =  urpc_data_get (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA1, &size1);
  if (data1 != NULL)
    {
      if (size1 != urpc_data_get_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE1))
        goto exit;
    }

  data2 =  urpc_data_get (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA2, &size2);
  if (data2 != NULL)
    {
      if (size2 != urpc_data_get_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE2))
        goto exit;
    }

  status = hyscan_cache_set2i (cache_server->cache, key, detail,
                               data1, size1, data2, size2);

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS,
                        status ? HYSCAN_CACHE_RPC_STATUS_OK : HYSCAN_CACHE_RPC_STATUS_FAIL);
  return 0;
}

/* RPC функция HYSCAN_CACHE_RPC_PROC_GET. */
static gint
hyscan_cache_server_rpc_proc_get (guint32   session,
                                  uRpcData *urpc_data,
                                  void     *proc_data,
                                  void     *key_data)
{
  HyScanCacheServer *cache_server = proc_data;

  guint64  key;
  guint64  detail;
  gpointer data1;
  guint32  size1;
  gpointer data2;
  guint32  size2;
  gboolean status = FALSE;

  key = urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY);
  detail = urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL);
  if (key == 0)
    goto exit;

  data1 = NULL;
  size1 = urpc_data_get_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE1);
  if (size1 > 0)
    {
      data1 = urpc_data_set (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA1, NULL, size1);
      if (data1 == NULL)
        goto exit;
    }
  else
    {
      goto exit;
    }

  data2 = NULL;
  size2 = urpc_data_get_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE2);
  if (size2 > 0)
    {
      data2 = urpc_data_set (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA2, NULL, size2);
      if (data2 == NULL)
        goto exit;
    }

  status = hyscan_cache_get2i (cache_server->cache, key, detail,
                               data1, &size1, data2, &size2);
  if (status)
    {
      if (urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE1, size1) != 0)
        goto exit;
      if (data2 != NULL)
        {
          if (urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE2, size2) != 0)
            goto exit;
        }
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS,
                        status ? HYSCAN_CACHE_RPC_STATUS_OK : HYSCAN_CACHE_RPC_STATUS_FAIL);
  return 0;
}

/* RPC функция HYSCAN_CACHE_RPC_PROC_SIZE. */
static gint
hyscan_cache_server_rpc_proc_size (guint32   session,
                                   uRpcData *urpc_data,
                                   void     *proc_data,
                                   void     *key_data)
{
  HyScanCacheServer *cache_server = proc_data;

  guint64  key;
  guint64  detail;
  guint32  size;
  gboolean status = FALSE;

  key = urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY);
  detail = urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL);
  if (key == 0)
    goto exit;

  status = hyscan_cache_get2i (cache_server->cache, key, detail,
                               NULL, &size, NULL, NULL);
  if (status)
    {
      if (urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE1, size) != 0)
        status = FALSE;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS,
                        status ? HYSCAN_CACHE_RPC_STATUS_OK : HYSCAN_CACHE_RPC_STATUS_FAIL);
  return 0;
}

HyScanCacheServer *
hyscan_cache_server_new (const gchar *uri,
                         guint32      size,
                         guint32      n_threads,
                         guint32      n_clients)
{
  return g_object_new (HYSCAN_TYPE_CACHE_SERVER, "uri", uri, "size", size,
                                                 "n-threads", n_threads, "n-clients", n_clients,
                                                 NULL);
}
