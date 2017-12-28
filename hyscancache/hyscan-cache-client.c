/*
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

#define hyscan_cache_client_lock_error()   do { \
                                             g_warning ("%s: can't lock rpc transport to '%s'", __FUNCTION__, priv->uri); \
                                             goto exit; \
                                           } while (0)

#define hyscan_cache_client_set_error(p)   do { \
                                             g_warning ("%s: can't set '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_cache_client_get_error(p)   do { \
                                             g_warning ("%s: can't get '%s' value", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

#define hyscan_cache_client_exec_error(p)  do { \
                                             g_warning ("%s: can't execute '%s' procedure", __FUNCTION__, p); \
                                             goto exit; \
                                           } while (0)

enum
{
  PROP_O,
  PROP_URI
};

/* Внутренние данные объекта. */
struct _HyScanCacheClientPrivate
{
  gchar               *uri;
  uRpcClient          *rpc;
};

static void    hyscan_cache_client_interface_init      (HyScanCacheInterface *iface);
static void    hyscan_cache_client_set_property        (GObject              *object,
                                                        guint                 prop_id,
                                                        const GValue         *value,
                                                        GParamSpec           *pspec);
static void    hyscan_cache_client_object_constructed  (GObject              *object);
static void    hyscan_cache_client_object_finalize     (GObject              *object);

G_DEFINE_TYPE_WITH_CODE (HyScanCacheClient, hyscan_cache_client, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanCacheClient)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_CACHE, hyscan_cache_client_interface_init));

static void
hyscan_cache_client_class_init (HyScanCacheClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_cache_client_set_property;
  object_class->constructed = hyscan_cache_client_object_constructed;
  object_class->finalize = hyscan_cache_client_object_finalize;

  g_object_class_install_property (object_class, PROP_URI,
                                   g_param_spec_string ("uri", "Uri", "HyScan cache uri", NULL,
                                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_cache_client_init (HyScanCacheClient *cachec)
{
  cachec->priv = hyscan_cache_client_get_instance_private (cachec);
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
      cachec->priv->uri = g_value_dup_string (value);
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
  HyScanCacheClientPrivate *priv = cachec->priv;
  uRpcData *urpc_data;
  guint32 version;

  /* Подключаемся к RPC серверу. */
  priv->rpc = urpc_client_create (priv->uri, URPC_MAX_DATA_SIZE, URPC_DEFAULT_DATA_TIMEOUT);
  if (priv->rpc == NULL)
    return;

  if (urpc_client_connect (priv->rpc) != 0)
    {
      g_warning ("HyScanCacheClient: can't connect to '%s'", priv->uri);
      goto fail;
    }

  /* Проверка версии сервера кэша данных. */
  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    {
      g_warning ("HyScanCacheClient: can't lock rpc transport to '%s'", priv->uri);
      goto fail;
    }

  if (urpc_client_exec (priv->rpc, HYSCAN_CACHE_RPC_PROC_VERSION) != URPC_STATUS_OK)
    {
      urpc_client_unlock (priv->rpc);
      g_warning ("HyScanCacheClient: can't execute procedure");
      goto fail;
    }

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_VERSION, &version) != 0 ||
      version != HYSCAN_CACHE_RPC_VERSION)
    {
      g_warning ("HyScanCacheClient: server version mismatch: need %d, got: %d",
                 HYSCAN_CACHE_RPC_VERSION, version);
      urpc_client_unlock (priv->rpc);
      goto fail;
    }

  urpc_client_unlock (priv->rpc);
  return;

fail:
  urpc_client_destroy (priv->rpc);
  priv->rpc = NULL;
}

static void
hyscan_cache_client_object_finalize (GObject *object)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (object);
  HyScanCacheClientPrivate *priv = cachec->priv;

  if (priv->rpc != NULL)
    urpc_client_destroy (priv->rpc);

  g_free (priv->uri);

  G_OBJECT_CLASS (hyscan_cache_client_parent_class)->finalize (object);
}

/* Все функции этого класса реализованы одинаково:
    - блокируется канал передачи данных RPC;
    - устанавливаются значения параметров вызываемой функции;
    - производится вызов RPC функции;
    - считывается результат вызова функции;
    - освобождается канал передачи. */

/* Функция добавляет или изменяет объект в кэше. */
static gboolean
hyscan_cache_client_set (HyScanCache  *cache,
                         guint64       key,
                         guint64       detail,
                         HyScanBuffer *buffer1,
                         HyScanBuffer *buffer2)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (cache);
  HyScanCacheClientPrivate *priv = cachec->priv;
  uRpcData *urpc_data;
  guint32 exec_status;

  guint8 *data;
  gpointer data1 = NULL;
  gpointer data2 = NULL;
  guint32 size1 = 0;
  guint32 size2 = 0;

  gboolean status = FALSE;

  if (buffer1 != NULL)
    data1 = hyscan_buffer_get_data (buffer1, &size1);
  if (buffer2 != NULL)
    data2 = hyscan_buffer_get_data (buffer2, &size2);

  if (priv->rpc == NULL)
    return FALSE;

  if ((size1 + size2) > URPC_MAX_DATA_SIZE - 1024 )
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_cache_client_lock_error ();

  if (urpc_data_set_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY, key) != 0)
    hyscan_cache_client_set_error ("key");

  if (urpc_data_set_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL, detail) != 0)
    hyscan_cache_client_set_error ("detail");

  data = urpc_data_set (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA, NULL, size1 + size2);
  if (data == NULL)
    hyscan_cache_client_set_error ("data");

  if (size1 > 0 && data1 != NULL)
    memcpy (data, data1, size1);

  if (size2 > 0 && data2 != NULL)
    memcpy (data + size1, data2, size2);

  if (urpc_client_exec (priv->rpc, HYSCAN_CACHE_RPC_PROC_SET) != URPC_STATUS_OK)
    hyscan_cache_client_exec_error ("set");

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_cache_client_get_error ("exec_status");
  if (exec_status != HYSCAN_CACHE_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);

  return status;
}

/* Функция считывает объект из кэша. */
gboolean
hyscan_cache_client_get (HyScanCache  *cache,
                         guint64       key,
                         guint64       detail,
                         guint32       size1,
                         HyScanBuffer *buffer1,
                         HyScanBuffer *buffer2)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (cache);
  HyScanCacheClientPrivate *priv = cachec->priv;
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;
  guint8 *data;
  guint32 size;

  if (priv->rpc == NULL)
    return FALSE;

  if (buffer1 == NULL && buffer2 != NULL)
    return FALSE;

  urpc_data = urpc_client_lock (priv->rpc);
  if (urpc_data == NULL)
    hyscan_cache_client_lock_error ();

  if (urpc_data_set_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY, key) != 0)
    hyscan_cache_client_set_error ("key");

  if (urpc_data_set_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL, detail) != 0)
    hyscan_cache_client_set_error ("detail");

  if (urpc_client_exec (priv->rpc, HYSCAN_CACHE_RPC_PROC_GET) != URPC_STATUS_OK)
    hyscan_cache_client_exec_error ("get");

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_cache_client_get_error ("exec_status");
  if (exec_status != HYSCAN_CACHE_RPC_STATUS_OK)
    goto exit;

  data = urpc_data_get (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA, &size);
  if (data == NULL)
    hyscan_cache_client_get_error ("data");

  size1 = MIN (size, size1);
  hyscan_buffer_set_data (buffer1, HYSCAN_DATA_BLOB, data, size1);

  if (buffer2 != NULL)
    hyscan_buffer_set_data (buffer2, HYSCAN_DATA_BLOB, data + size1, size - size1);

  status = TRUE;

exit:
  urpc_client_unlock (priv->rpc);

  return status;
}

/* Функция создаёт новый объект HyScanCacheClient. */
HyScanCacheClient *
hyscan_cache_client_new (const gchar *uri)
{
  return g_object_new (HYSCAN_TYPE_CACHE_CLIENT,
                       "uri", uri,
                       NULL);
}

static void
hyscan_cache_client_interface_init (HyScanCacheInterface *iface)
{
  iface->set = hyscan_cache_client_set;
  iface->get = hyscan_cache_client_get;
}
