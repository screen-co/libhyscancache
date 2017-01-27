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
                                             g_warning ("%s: can't lock rpc transport to '%s'", __FUNCTION__, cachec->uri); \
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
  PROP_SERVER_NAME
};

/* Внутренние данные объекта. */
struct _HyScanCacheClient
{
  GObject              parent_instance;

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
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_CACHE, hyscan_cache_client_interface_init));

static void
hyscan_cache_client_class_init (HyScanCacheClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->set_property = hyscan_cache_client_set_property;
  object_class->constructed = hyscan_cache_client_object_constructed;
  object_class->finalize = hyscan_cache_client_object_finalize;

  g_object_class_install_property (object_class, PROP_SERVER_NAME,
                                   g_param_spec_string ("server-name", "Server name", "Server name", NULL,
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
    case PROP_SERVER_NAME:
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
  uRpcData *urpc_data;
  guint32 version;

  /* Подключаемся к RPC серверу. */
  cachec->rpc = urpc_client_create (cachec->uri, URPC_MAX_DATA_SIZE, URPC_DEFAULT_DATA_TIMEOUT);
  if (cachec->rpc == NULL)
    return;

  if (urpc_client_connect (cachec->rpc) != 0)
    {
      g_warning ("hyscan_cache_client: can't connect to '%s'", cachec->uri);
      goto fail;
    }

  /* Проверка версии сервера. */
  urpc_data = urpc_client_lock (cachec->rpc);
  if (urpc_data == NULL)
    {
      g_warning ("hyscan_cache_client: can't lock rpc transport to '%s'", cachec->uri);
      goto fail;
    }

  if (urpc_client_exec (cachec->rpc, HYSCAN_CACHE_RPC_PROC_VERSION) != URPC_STATUS_OK)
    {
      urpc_client_unlock (cachec->rpc);
      g_warning ("hyscan_cache_client: can't execute procedure");
      goto fail;
    }

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_VERSION, &version) != 0 ||
      version != HYSCAN_CACHE_RPC_VERSION)
    {
      g_warning ("hyscan_cache_client: server version mismatch: need %d, got: %d",
                 HYSCAN_CACHE_RPC_VERSION, version);
      urpc_client_unlock (cachec->rpc);
      goto fail;
    }

  urpc_client_unlock (cachec->rpc);
  return;

fail:
  urpc_client_destroy (cachec->rpc);
  cachec->rpc = NULL;
}

static void
hyscan_cache_client_object_finalize (GObject *object)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (object);

  if (cachec->rpc != NULL)
    urpc_client_destroy (cachec->rpc);

  g_free (cachec->uri);

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
hyscan_cache_client_set (HyScanCache *cache,
                         guint64      key,
                         guint64      detail,
                         gpointer     data1,
                         guint32      size1,
                         gpointer     data2,
                         guint32      size2)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (cache);
  uRpcData *urpc_data;
  guint32 exec_status;

  gboolean status = FALSE;

  if (cachec->rpc == NULL)
    return FALSE;

  if ((size1 + size2) > URPC_MAX_DATA_SIZE - 1024 )
    return FALSE;

  urpc_data = urpc_client_lock (cachec->rpc);
  if (urpc_data == NULL)
    hyscan_cache_client_lock_error ();

  if (urpc_data_set_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_KEY, key) != 0)
    hyscan_cache_client_set_error ("key");

  if (urpc_data_set_uint64 (urpc_data, HYSCAN_CACHE_RPC_PARAM_DETAIL, detail) != 0)
    hyscan_cache_client_set_error ("detail");

  if (size1 > 0 && data1 != NULL)
    {
      if (urpc_data_set_int32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE1, size1) != 0)
        hyscan_cache_client_set_error ("size1");
      if (urpc_data_set (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA1, data1, size1) == NULL)
        hyscan_cache_client_set_error ("data1");
    }

  if (size2 > 0 && data2 != NULL)
    {
      if (urpc_data_set_int32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_SIZE2, size2) != 0)
        hyscan_cache_client_set_error ("size2");
      if (urpc_data_set (urpc_data, HYSCAN_CACHE_RPC_PARAM_DATA2, data2, size2) == NULL)
        hyscan_cache_client_set_error ("data2");
    }

  if (urpc_client_exec (cachec->rpc, HYSCAN_CACHE_RPC_PROC_SET) != URPC_STATUS_OK)
    hyscan_cache_client_exec_error ("set");

  if (urpc_data_get_uint32 (urpc_data, HYSCAN_CACHE_RPC_PARAM_STATUS, &exec_status) != 0)
    hyscan_cache_client_get_error ("exec_status");
  if (exec_status != HYSCAN_CACHE_RPC_STATUS_OK)
    goto exit;

  status = TRUE;

exit:
  urpc_client_unlock (cachec->rpc);

  return status;
}

/* Функция считывает объект из кэша. */
gboolean
hyscan_cache_client_get (HyScanCache *cache,
                         guint64      key,
                         guint64      detail,
                         gpointer     buffer1,
                         guint32     *buffer1_size,
                         gpointer     buffer2,
                         guint32     *buffer2_size)
{
  HyScanCacheClient *cachec = HYSCAN_CACHE_CLIENT (cache);
  uRpcData *data;
  guint32 exec_status;

  gboolean status = FALSE;
  gpointer value;
  guint32 value_size;
  guint32 size1;
  guint32 size2;

  if (cachec->rpc == NULL)
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
    hyscan_cache_client_lock_error ();

  if (urpc_data_set_uint64 (data, HYSCAN_CACHE_RPC_PARAM_KEY, key) != 0)
    hyscan_cache_client_set_error ("key");

  if (urpc_data_set_uint64 (data, HYSCAN_CACHE_RPC_PARAM_DETAIL, detail) != 0)
    hyscan_cache_client_set_error ("detail");

  /* Запрашивается размер данных. */
  if (buffer1 == NULL && buffer2 == NULL)
    {
      if (urpc_client_exec (cachec->rpc, HYSCAN_CACHE_RPC_PROC_SIZE) != URPC_STATUS_OK)
        hyscan_cache_client_exec_error ("size");

      if (urpc_data_get_uint32 (data, HYSCAN_CACHE_RPC_PARAM_STATUS, &exec_status) != 0)
        hyscan_cache_client_get_error ("exec_status");
      if (exec_status != HYSCAN_CACHE_RPC_STATUS_OK)
        goto exit;

      if (buffer1_size != NULL)
        {
          if (urpc_data_get_uint32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE1, buffer1_size) != 0)
            goto exit;
        }
    }

  /* Запрашиваются данные. */
  else
    {
      if (urpc_data_set_int32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE1, *buffer1_size) != 0)
        hyscan_cache_client_set_error ("buffer1_size");

      if (buffer2 != NULL)
        {
          if (urpc_data_set_int32 (data, HYSCAN_CACHE_RPC_PARAM_SIZE2, *buffer2_size) != 0)
            hyscan_cache_client_set_error ("buffer2_size");
        }

      if (urpc_client_exec (cachec->rpc, HYSCAN_CACHE_RPC_PROC_GET) != URPC_STATUS_OK)
        hyscan_cache_client_exec_error ("get");

      if (urpc_data_get_uint32 (data, HYSCAN_CACHE_RPC_PARAM_STATUS, &exec_status) != 0)
        hyscan_cache_client_get_error ("exec_status");
      if (exec_status != HYSCAN_CACHE_RPC_STATUS_OK)
        goto exit;

      if (urpc_data_get_uint32(data, HYSCAN_CACHE_RPC_PARAM_SIZE1, &size1) != 0)
        hyscan_cache_client_get_error ("size1");

      if (urpc_data_get_uint32(data, HYSCAN_CACHE_RPC_PARAM_SIZE2, &size2) != 0)
        size2 = 0;

      value = urpc_data_get (data, HYSCAN_CACHE_RPC_PARAM_DATA1, &value_size);
      if (value == NULL || value_size != size1 + size2)
        hyscan_cache_client_get_error ("data-size");

      if (size1 > *buffer1_size)
        goto exit;
      memcpy (buffer1, value, size1);
      *buffer1_size = size1;

      if (buffer2 != NULL)
        {
          if (size2 > *buffer2_size)
            goto exit;
          memcpy (buffer2, (guint8*)value + size1, size2);
          *buffer2_size = size2;
        }

      status = TRUE;
    }

exit:
  urpc_client_unlock (cachec->rpc);

  return status;
}

/* Функция создаёт новый объект HyScanCacheClient. */
HyScanCacheClient *
hyscan_cache_client_new (const gchar *server_name)
{
  return g_object_new (HYSCAN_TYPE_CACHE_CLIENT, "server-name", server_name, NULL);
}

static void
hyscan_cache_client_interface_init (HyScanCacheInterface *iface)
{
  iface->set = hyscan_cache_client_set;
  iface->get = hyscan_cache_client_get;
}
