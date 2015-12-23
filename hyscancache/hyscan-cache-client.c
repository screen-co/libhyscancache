/**
 * \file hyscan-cache-client.c
 *
 * \brief Исходный файл клиента системы кэширования данных на сервере
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-cache-client.h"
#include "hyscan-cache-rpc.h"

#include <string.h>
#include <urpc-client.h>

#define hyscan_cache_client_lock_error(f)      do { \
                                                 g_warning ("%s: can't lock rpc transport to '%s'", f, cachec->uri); \
                                                 goto exit; \
                                               } while (0)

#define hyscan_cache_client_set_error(f,p)     do { \
                                                 g_warning ("%s: can't set '%s' value", f, p); \
                                                 goto exit; \
                                               } while (0)

#define hyscan_cache_client_exec_error(f,p)    do { \
                                                 g_warning ("%s: can't execute '%s' procedure", f, p); \
                                                 goto exit; \
                                               } while (0)

enum
{
  PROP_O,
  PROP_URI
};

/* Внутренние данные объекта. */
struct _HyScanCacheClient
{
  GObject              parent_instance;

  gchar               *uri;
  uRpcClient          *rpc;
};

static void            hyscan_cache_client_interface_init          (HyScanCacheInterface *iface);
static void            hyscan_cache_client_set_property            (GObject              *object,
                                                                   guint                 prop_id,
                                                                   const GValue         *value,
                                                                   GParamSpec           *pspec);
static void            hyscan_cache_client_object_constructed      (GObject              *object);
static void            hyscan_cache_client_object_finalize         (GObject              *object);

G_DEFINE_TYPE_WITH_CODE (HyScanCacheClient, hyscan_cache_client, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_CACHE, hyscan_cache_client_interface_init));

static void
hyscan_cache_client_class_init (HyScanCacheClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->set_property = hyscan_cache_client_set_property;
  object_class->constructed = hyscan_cache_client_object_constructed;
  object_class->finalize = hyscan_cache_client_object_finalize;

  g_object_class_install_property (object_class, PROP_URI,
                                   g_param_spec_string ("uri", "Uri", "Cache uri", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_cache_client_init (HyScanCacheClient *cache)
{
}

static void
hyscan_cache_client_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (object);

  switch (prop_id)
    {
    case PROP_URI:
      cachec->uri = g_strdup_printf ("shm://%s", g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_cache_client_object_constructed (GObject *object)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (object);
  uRpcData *data;
  guint32 version;

  /* Подключаемся к RPC серверу. */
  cachec->rpc = urpc_client_create (cachec->uri, URPC_MAX_DATA_SIZE, URPC_DEFAULT_DATA_TIMEOUT);
  if (urpc_client_connect (cachec->rpc) < 0)
    {
      g_warning ("hyscan_cache_client: can't connect to '%s'", cachec->uri);
      urpc_client_destroy (cachec->rpc);
      cachec->rpc = NULL;
      return;
    }

  /* Проверяем версию сервера. */
  data = urpc_client_lock (cachec->rpc);
  if (data == NULL)
    hyscan_cache_client_lock_error ("hyscan_cache_client");

  if (urpc_client_exec (cachec->rpc, HYSCAN_CACHE_RPC_PROC_VERSION) != URPC_STATUS_OK)
    hyscan_cache_client_exec_error ("hyscan_cache_client", "version");

  version = urpc_data_get_uint32 (data, HYSCAN_CACHE_RPC_PARAM_VERSION);

exit:
  urpc_client_unlock (cachec->rpc);

  if (version != HYSCAN_CACHE_RPC_VERSION)
    {
      g_warning ("hyscan_cache_client: server version mismatch: need %d, got: %d",
                 HYSCAN_CACHE_RPC_VERSION, version);
      urpc_client_destroy (cachec->rpc);
      cachec->rpc = NULL;
      return;
    }
}

static void
hyscan_cache_client_object_finalize (GObject *object)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (object);

  g_free (cachec->uri);
  if (cachec->rpc != NULL)
    urpc_client_destroy (cachec->rpc);

  G_OBJECT_CLASS (hyscan_cache_client_parent_class)->finalize (object);
}

/* Функция создаёт новый объект HyScanCacheClient. */
HyScanCacheClient *
hyscan_cache_client_new (gchar *uri)
{
  return g_object_new (HYSCAN_TYPE_CACHE_CLIENT, "uri", uri, NULL);
}

/* Функция добавляет или изменяет объект в кэше. */
gboolean
hyscan_cache_client_set2 (HyScanCache *cache,
                          guint64      key,
                          guint64      detail,
                          gpointer     data1,
                          guint32      size1,
                          gpointer     data2,
                          guint32      size2)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (cache);

  uRpcData *data;
  gboolean status = FALSE;

  if ((size1 + size2) > URPC_MAX_DATA_SIZE - 1024 )
    return FALSE;

  data = urpc_client_lock (cachec->rpc);
  if (data == NULL)
    hyscan_cache_client_lock_error ("hyscan_cache_client_set2");

  if (urpc_data_set_uint64 (data, HYSCAN_CACHE_RPC_PARAM_KEY, key) < 0)
    hyscan_cache_client_set_error ("hyscan_cache_client_set2", "key");

  if (urpc_data_set_uint64 (data, HYSCAN_CACHE_RPC_PARAM_DETAIL, detail) < 0)
    hyscan_cache_client_set_error ("hyscan_cache_client_set2", "detail");

  if (size1 > 0 && data1 != NULL)
    {
      if (urpc_data_set_uint32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE1, size1) < 0)
        hyscan_cache_client_set_error ("hyscan_cache_client_set2", "size1");
      if (urpc_data_set (data, HYSCAN_CACHE_RPC_PARAM_DATA1, data1, size1) == NULL)
        hyscan_cache_client_set_error ("hyscan_cache_client_set2", "data1");
    }

  if (size2 > 0 && data2 != NULL)
    {
      if (urpc_data_set_uint32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE2, size2) < 0)
        hyscan_cache_client_set_error ("hyscan_cache_client_set2", "size2");
      if (urpc_data_set (data, HYSCAN_CACHE_RPC_PARAM_DATA2, data2, size2) == NULL)
        hyscan_cache_client_set_error ("hyscan_cache_client_set2", "data2");
    }

  if (urpc_client_exec (cachec->rpc, HYSCAN_CACHE_RPC_PROC_SET) != URPC_STATUS_OK)
    hyscan_cache_client_exec_error ("hyscan_cache_client_set2", "set");

  if (urpc_data_get_uint32 (data, HYSCAN_CACHE_RPC_PARAM_STATUS) == HYSCAN_CACHE_RPC_STATUS_OK)
    status = TRUE;

exit:
  urpc_client_unlock (cachec->rpc);

  return status;
}

/* Функция добавляет или изменяет объект в кэше. */
gboolean
hyscan_cache_client_set (HyScanCache *cache,
                         guint64      key,
                         guint64      detail,
                         gpointer     data,
                         guint32      size)
{
  return hyscan_cache_client_set2 (cache, key, detail, data, size, NULL, 0);
}

/* Функция считывает объект из кэша. */
gboolean
hyscan_cache_client_get2 (HyScanCache *cache,
                          guint64      key,
                          guint64      detail,
                          gpointer     buffer1,
                          guint32     *buffer1_size,
                          gpointer     buffer2,
                          guint32     *buffer2_size)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (cache);

  uRpcData *data;
  gpointer value;
  guint32 value_size;
  gboolean status = FALSE;

  if (buffer1 != NULL && *buffer1_size == 0)
    return FALSE;
  if (buffer2 != NULL && *buffer2_size == 0)
    return FALSE;
  if (buffer1 == NULL && buffer2 != NULL)
    return FALSE;

  if (buffer1 != NULL && buffer2 == NULL)
    {
      if (*buffer1_size > URPC_MAX_DATA_SIZE - 1024)
        return FALSE;
    }
  if (buffer1 != NULL && buffer2 != NULL)
    {
      if ((*buffer1_size + *buffer2_size) > URPC_MAX_DATA_SIZE - 1024)
        return FALSE;
    }

  data = urpc_client_lock (cachec->rpc);
  if (data == NULL)
    hyscan_cache_client_lock_error ("hyscan_cache_client_get2");

  if (urpc_data_set_uint64 (data, HYSCAN_CACHE_RPC_PARAM_KEY, key) < 0)
    hyscan_cache_client_set_error ("hyscan_cache_client_get2", "key");

  if (urpc_data_set_uint64 (data, HYSCAN_CACHE_RPC_PARAM_DETAIL, detail) < 0)
    hyscan_cache_client_set_error ("hyscan_cache_client_get2", "detail");

  /* Запрашивается размер данных. */
  if (buffer1 == NULL && buffer2 == NULL)
    {
      if (urpc_client_exec (cachec->rpc, HYSCAN_CACHE_RPC_PROC_SIZE) != URPC_STATUS_OK)
        hyscan_cache_client_exec_error ("hyscan_cache_client_get2", "size");

      if (urpc_data_get_uint32 (data, HYSCAN_CACHE_RPC_PARAM_STATUS) != HYSCAN_CACHE_RPC_STATUS_OK)
        goto exit;

      if (buffer1_size != NULL)
        *buffer1_size = urpc_data_get_int32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE1);
    }

  /* Запрашиваются данные. */
  else
    {
      if (urpc_data_set_uint32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE1, *buffer1_size) < 0)
        hyscan_cache_client_set_error ("hyscan_cache_client_get2", "buffer1_size");

      if (buffer2 != NULL)
        {
          if (urpc_data_set_uint32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE2, *buffer2_size) < 0)
            hyscan_cache_client_set_error ("hyscan_cache_client_get2", "buffer2_size");
        }

      if (urpc_client_exec (cachec->rpc, HYSCAN_CACHE_RPC_PROC_GET) != URPC_STATUS_OK)
        hyscan_cache_client_exec_error ("hyscan_cache_client_get2", "get");

      if (urpc_data_get_uint32 (data, HYSCAN_CACHE_RPC_PARAM_STATUS) != HYSCAN_CACHE_RPC_STATUS_OK)
        goto exit;

      value = urpc_data_get (data, HYSCAN_CACHE_RPC_PARAM_DATA1, NULL);
      value_size = urpc_data_get_uint32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE1);
      if (value == NULL || value_size > *buffer1_size)
        goto exit;
      memcpy (buffer1, value, value_size);
      *buffer1_size = value_size;

      if (buffer2 != NULL)
        {
          value = urpc_data_get (data, HYSCAN_CACHE_RPC_PARAM_DATA2, NULL);
          value_size = urpc_data_get_uint32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE2);
          if (value == NULL || value_size > *buffer2_size)
            goto exit;
          memcpy (buffer2, value, value_size);
          *buffer2_size = value_size;
        }

      status = TRUE;
    }

exit:
  urpc_client_unlock (cachec->rpc);

  return status;
}

/* Функция считывает объект из кэша. */
gboolean
hyscan_cache_client_get (HyScanCache *cache,
                         guint64      key,
                         guint64      detail,
                         gpointer     buffer,
                         guint32     *buffer_size)
{
  return hyscan_cache_client_get2 (cache, key,detail, buffer, buffer_size, NULL, NULL);
}

static void
hyscan_cache_client_interface_init (HyScanCacheInterface *iface)
{
  iface->set = hyscan_cache_client_set;
  iface->set2 = hyscan_cache_client_set2;
  iface->get = hyscan_cache_client_get;
  iface->get2 = hyscan_cache_client_get2;
}
