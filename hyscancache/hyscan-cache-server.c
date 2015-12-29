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
  PROP_NAME,
  PROP_SIZE,
  PROP_N_THREADS,
  PROP_N_CLIENTS
};

struct _HyScanCacheServer
{
  GObject              parent_instance;

  volatile gint        running;

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
  object_class->finalize = hyscan_cache_server_object_finalize;

  g_object_class_install_property (object_class, PROP_NAME,
                                   g_param_spec_string ("name", "Name", "Server name", NULL,
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
    case PROP_NAME:
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
hyscan_cache_server_object_finalize (GObject *object)
{
  HyScanCacheServer *cache_server = HYSCAN_CACHE_SERVER (object);

  g_free (cache_server->uri);

  if (cache_server->rpc != NULL)
    urpc_server_destroy (cache_server->rpc);

  if (cache_server->cache != NULL)
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
  guint32 rpc_status = HYSCAN_CACHE_RPC_STATUS_FAIL;

  guint64  key;
  guint64  detail;
  gpointer data1;
  guint32  size1;
  gpointer data2;
  guint32  size2;
  gboolean status = FALSE;

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY, &key) != 0)
    hyscan_cache_server_get_error ("key");

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL, &detail) != 0)
    detail = 0;

  data1 =  urpc_data_get (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA1, &size1);
  data2 =  urpc_data_get (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA2, &size2);
  status = hyscan_cache_set2i (cache_server->cache, key, detail,
                               data1, size1, data2, size2);
  if (status)
    rpc_status = HYSCAN_CACHE_RPC_STATUS_OK;

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS, rpc_status);
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
  guint32 rpc_status = HYSCAN_CACHE_RPC_STATUS_FAIL;

  guint64  key;
  guint64  detail;
  gpointer data1;
  gint32  size1;
  gint32  size2;
  guint32  size;
  gboolean status = FALSE;

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY, &key) != 0)
    hyscan_cache_server_get_error ("key");

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL, &detail) != 0)
    detail = 0;

  size = 0;
  if (urpc_data_get_int32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE1, &size1) == 0)
    size += size1;
  else
    hyscan_cache_server_get_error ("size1");

  if (urpc_data_get_int32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE2, &size2) == 0)
    size += size2;
  else
    size2 = 0;

  data1 = urpc_data_set (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA1, NULL, size);
  if (data1 == NULL)
    hyscan_cache_server_set_error ("data");

  status = hyscan_cache_get2i (cache_server->cache, key, detail,
                               data1, &size1, data1 + size1, &size2);
  if (status)
    {
      size = size1 + size2;
      if (urpc_data_set (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA1, NULL, size) == NULL)
        hyscan_cache_server_set_error ("data-size");

      if (urpc_data_set_int32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE1, size1) != 0)
        hyscan_cache_server_set_error ("size1");
      if (size2 > 0 )
        {
          if (urpc_data_set_int32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE2, size2) != 0)
            hyscan_cache_server_set_error ("size2");
        }
      rpc_status = HYSCAN_CACHE_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS, rpc_status);
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
  guint32 rpc_status = HYSCAN_CACHE_RPC_STATUS_FAIL;

  guint64  key;
  guint64  detail;
  gint32  size;
  gboolean status = FALSE;

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY, &key) != 0)
    hyscan_cache_server_get_error ("key");

  if (urpc_data_get_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL, &detail) != 0)
    detail = 0;

  status = hyscan_cache_get2i (cache_server->cache, key, detail,
                               NULL, &size, NULL, NULL);
  if (status)
    {
      if (urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE1, size) == 0)
        rpc_status = HYSCAN_CACHE_RPC_STATUS_OK;
    }

exit:
  urpc_data_set_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS, rpc_status);
  return 0;
}

HyScanCacheServer *
hyscan_cache_server_new (const gchar *name,
                         guint32      size,
                         guint32      n_threads,
                         guint32      n_clients)
{
  return g_object_new (HYSCAN_TYPE_CACHE_SERVER, "name", name, "size", size,
                                                 "n-threads", n_threads, "n-clients", n_clients,
                                                 NULL);
}

gboolean
hyscan_cache_server_start (HyScanCacheServer *cache_server)
{
  gint status;

  cache_server->cache = HYSCAN_CACHE (hyscan_cached_new (cache_server->size));

  cache_server->rpc = urpc_server_create (cache_server->uri, cache_server->n_threads, cache_server->n_clients,
                                          URPC_DEFAULT_SESSION_TIMEOUT, URPC_MAX_DATA_SIZE, URPC_DEFAULT_DATA_TIMEOUT);
  if (cache_server->rpc == NULL)
    return FALSE;

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

  return TRUE;

fail:
  urpc_server_destroy (cache_server->rpc);
  cache_server->rpc = NULL;
  return FALSE;
}
