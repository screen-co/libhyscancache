/*
 * \file hyscan-cached.c
 *
 * \brief Исоходный файл системы кэширования данных в оперативной памяти
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-cached.h"

#include <string.h>
#include <stdlib.h>

#ifdef CPU_ARCH_X32
  #define MIN_CACHE_SIZE   64
  #define MAX_CACHE_SIZE   2048
#else
  #define MIN_CACHE_SIZE   64
  #define MAX_CACHE_SIZE   131072
#endif

#define OBJECT_HEADER_SIZE offsetof (ObjectInfo, data)

enum
{
  PROP_O,
  PROP_CACHE_SIZE
};

/* Информация об объекте. */
typedef struct _ObjectInfo ObjectInfo;
struct _ObjectInfo
{
  ObjectInfo          *prev;                   /* Указатель на предыдущий объект. */
  ObjectInfo          *next;                   /* Указатель на следующий объект. */

  guint64              hash;                   /* Хэш идентификатора объекта. */
  guint64              detail;                 /* Хэш дополнительной информации объекта. */

  guint32              allocated;              /* Размер буфера. */
  guint32              size;                   /* Размер объекта. */
  gint8                data[];                 /* Данные объекта. */
};

/* Внутренние данные объекта. */
struct _HyScanCachedPrivate
{
  guint64              cache_size;             /* Максимальный размер данных в кэше. */
  guint64              used_size;              /* Текущий размер данных в кэше. */

  GHashTable          *objects;                /* Таблица объектов кэша. */

  ObjectInfo          *top_object;             /* Указатель на объект доступ к которому осуществлялся недавно. */
  ObjectInfo          *bottom_object;          /* Указатель на объект доступ к которому осуществлялся давно. */

  GRWLock              data_lock;              /* Блокировка доступа к данным. */
  GRWLock              list_lock;              /* Блокировка доступа к списку объектов. */
};

static void            hyscan_cached_interface_init               (HyScanCacheInterface *iface);
static void            hyscan_cached_set_property                 (GObject              *object,
                                                                   guint                 prop_id,
                                                                   const GValue         *value,
                                                                   GParamSpec           *pspec);
static void            hyscan_cached_object_constructed           (GObject              *object);
static void            hyscan_cached_object_finalize              (GObject              *object);

static void            hyscan_cached_free_used                    (HyScanCachedPrivate  *priv,
                                                                   guint32               size);

static ObjectInfo     *hyscan_cached_rise_object                  (HyScanCachedPrivate  *priv,
                                                                   guint64               key,
                                                                   guint64               detail,
                                                                   gpointer              data1,
                                                                   guint32               size1,
                                                                   gpointer              data2,
                                                                   guint32               size2);
static ObjectInfo     *hyscan_cached_update_object                (HyScanCachedPrivate  *priv,
                                                                   ObjectInfo           *object,
                                                                   guint64               detail,
                                                                   gpointer              data1,
                                                                   guint32               size1,
                                                                   gpointer              data2,
                                                                   guint32               size2);
static void            hyscan_cached_drop_object                  (HyScanCachedPrivate  *priv,
                                                                   ObjectInfo           *object);

static void            hyscan_cached_remove_object_from_used      (HyScanCachedPrivate  *priv,
                                                                   ObjectInfo           *object);
static void            hyscan_cached_place_object_on_top_of_used  (HyScanCachedPrivate  *priv,
                                                                   ObjectInfo           *object);

G_DEFINE_TYPE_WITH_CODE (HyScanCached, hyscan_cached, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (HyScanCached)
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_CACHE, hyscan_cached_interface_init));

static void
hyscan_cached_class_init (HyScanCachedClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = hyscan_cached_set_property;
  object_class->constructed = hyscan_cached_object_constructed;
  object_class->finalize = hyscan_cached_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE_SIZE,
                                   g_param_spec_uint ("cache-size", "Cache size", "Cache size, Mb",
                                                      MIN_CACHE_SIZE, MAX_CACHE_SIZE, MIN_CACHE_SIZE,
                                                      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_cached_init (HyScanCached *cached)
{
  cached->priv = hyscan_cached_get_instance_private (cached);
}

static void
hyscan_cached_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  HyScanCached *cached = HYSCAN_CACHED (object);
  HyScanCachedPrivate *priv = cached->priv;

  switch (prop_id)
    {
    case PROP_CACHE_SIZE:
      priv->cache_size = g_value_get_uint (value);
      priv->cache_size *= 1024 * 1024;
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
hyscan_cached_object_constructed (GObject *object)
{
  HyScanCached *cached = HYSCAN_CACHED (object);
  HyScanCachedPrivate *priv = cached->priv;

  g_rw_lock_init (&priv->data_lock);
  g_rw_lock_init (&priv->list_lock);

  /* Таблица объектов кэша. */
  priv->objects = g_hash_table_new_full (g_int64_hash, g_int64_equal, NULL, g_free);
}

static void
hyscan_cached_object_finalize (GObject *object)
{
  HyScanCached *cached = HYSCAN_CACHED (object);
  HyScanCachedPrivate *priv = cached->priv;

  g_hash_table_unref (priv->objects);

  g_rw_lock_clear (&priv->list_lock);
  g_rw_lock_clear (&priv->data_lock);

  G_OBJECT_CLASS (hyscan_cached_parent_class)->finalize (object);
}

/* Функция освобождает память в кэше для размещения нового объекта. */
static void
hyscan_cached_free_used (HyScanCachedPrivate *priv,
                         guint32              size)
{
  ObjectInfo *object = priv->bottom_object;

  /* Удаляем объекты пока не наберём достаточного объёма свободной памяти. */
  while (object != NULL && priv->cache_size < (priv->used_size + size))
    {
      hyscan_cached_drop_object (priv, object);
      object = priv->bottom_object;
    }
}

/* Функция выбирает структуру с информацией об объекте из кучи свободных, выделяет память под объект
   и сохраняет данные. */
static ObjectInfo *
hyscan_cached_rise_object (HyScanCachedPrivate *priv,
                           guint64              key,
                           guint64              detail,
                           gpointer             data1,
                           guint32              size1,
                           gpointer             data2,
                           guint32              size2)
{
  ObjectInfo *object;
  guint32 size = size1 + size2;

  /* Инициализация. */
  object = g_malloc (OBJECT_HEADER_SIZE + size);
  object->next = NULL;
  object->prev = NULL;

  /* Хеш идентификатора объекта и дополнительной информации. */
  object->hash = key;
  object->detail = detail;
  object->size = size;

  /* Данные объекта. */
  object->allocated = size;
  priv->used_size += (OBJECT_HEADER_SIZE + size);
  memcpy (object->data, data1, size1);
  if (size2 > 0)
    memcpy ((gint8*) object->data + size1, data2, size2);

  return object;
}

/* Функция обновляет используемый объект. */
static ObjectInfo *
hyscan_cached_update_object (HyScanCachedPrivate *priv,
                             ObjectInfo          *object,
                             guint64              detail,
                             gpointer             data1,
                             guint32              size1,
                             gpointer             data2,
                             guint32              size2)
{
  guint32 size = size1 + size2;

  /* Если текущий размер объекта меньше нового размера или больше нового на 5%, выделяем память заново. */
  if (object->allocated < size || ((gdouble) size / (gdouble) object->allocated) < 0.95)
    {
      g_hash_table_steal (priv->objects, &object->hash);
      object = g_realloc (object, OBJECT_HEADER_SIZE + size);
      g_hash_table_insert (priv->objects, &object->hash, object);

      priv->used_size -= (OBJECT_HEADER_SIZE + object->allocated);
      object->allocated = size;
      priv->used_size += (OBJECT_HEADER_SIZE + size);
    }

  /* Новый размер объекта. */
  object->size = size;
  object->detail = detail;

  /* Данные объекта. */
  memcpy (object->data, data1, size1);
  if (size2 > 0)
    memcpy ((gint8*) object->data + size1, data2, size2);

  return object;
}

/* Функция удаляет объект из кеша и помещает структуру в кучу свободных. */
static void
hyscan_cached_drop_object (HyScanCachedPrivate *priv,
                           ObjectInfo          *object)
{
  hyscan_cached_remove_object_from_used (priv, object);

  priv->used_size -= (OBJECT_HEADER_SIZE + object->allocated);
  g_hash_table_remove (priv->objects, &object->hash);
}

/* Функция удаляет объект из списка используемых. */
static void
hyscan_cached_remove_object_from_used (HyScanCachedPrivate *priv,
                                       ObjectInfo          *object)
{
  /* Единственный объект в списке */
  if ((priv->top_object == priv->bottom_object) && (priv->top_object == object))
    {
      priv->top_object = NULL;
      priv->bottom_object = NULL;
      return;
    }

  /* Объект не в списке. */
  if (object->prev == NULL && object->next == NULL)
    return;

  /* Объект в списке. */
  if (object->prev != NULL && object->next != NULL)
    {
      object->prev->next = object->next;
      object->next->prev = object->prev;
      object->prev = NULL;
      object->next = NULL;
    }

  /* Первый объект в списке. */
  else if (object->prev == NULL)
    {
      priv->top_object = object->next;
      object->next->prev = NULL;
      object->next = NULL;
    }

  /* Последний объект в списке. */
  else
    {
      priv->bottom_object = object->prev;
      object->prev->next = NULL;
      object->prev = NULL;
    }
}

/* Функция перемещает объект на вершину списка часто используемых. */
static void
hyscan_cached_place_object_on_top_of_used (HyScanCachedPrivate *priv,
                                           ObjectInfo          *object)
{
  g_rw_lock_writer_lock (&priv->list_lock);

  /* "Вынимаем" объект из цепочки используемых. */
  hyscan_cached_remove_object_from_used (priv, object);

  /* Первый объект в кэше. */
  if ((priv->top_object == NULL) && (priv->bottom_object == NULL))
    {
      priv->top_object = object;
      priv->bottom_object = object;
    }

  /* "Вставляем" перед первым объектом в цепочке. */
  else
    {
      priv->top_object->prev = object;
      object->next = priv->top_object;
      priv->top_object = object;
    }

  g_rw_lock_writer_unlock (&priv->list_lock);
}

/* Функция создаёт новый объект HyScanCached. */
HyScanCached *
hyscan_cached_new (guint32 cache_size)
{
  return g_object_new (HYSCAN_TYPE_CACHED, "cache-size", cache_size, NULL);
}

/* Функция добавляет или изменяет объект в кэше. */
static gboolean
hyscan_cached_set (HyScanCache  *cache,
                   guint64       key,
                   guint64       detail,
                   HyScanBuffer *buffer1,
                   HyScanBuffer *buffer2)
{
  HyScanCached *cached = HYSCAN_CACHED (cache);
  HyScanCachedPrivate *priv = cached->priv;

  ObjectInfo *object;
  gpointer data1 = NULL;
  gpointer data2 = NULL;
  guint32 size1 = 0;
  guint32 size2 = 0;
  guint32 size;

  if (buffer1 != NULL)
    data1 = hyscan_buffer_get_data (buffer1, &size1);
  if (buffer2 != NULL)
    data2 = hyscan_buffer_get_data (buffer2, &size2);

  size = size1 + size2;

  /* Если размер нового объекта слишком большой, не сохраняем его. */
  if (size > priv->cache_size / 10)
    return FALSE;

  g_rw_lock_writer_lock (&priv->data_lock);

  /* Ищем объект в кэше. */
  object = g_hash_table_lookup (priv->objects, &key);

  /* Если размер объекта равен нулю, удаляем объект. */
  if (size == 0)
    {
      if (object != NULL)
        hyscan_cached_drop_object (priv, object);
      g_rw_lock_writer_unlock (&priv->data_lock);
      return TRUE;
    }

  /* Очищаем кэш если достигнут лимит используемой памяти. */
  if (priv->used_size + OBJECT_HEADER_SIZE + size > priv->cache_size)
    {
      hyscan_cached_free_used (priv, OBJECT_HEADER_SIZE + size);
      object = g_hash_table_lookup (priv->objects, &key);
    }

  /* Если объект уже был в кэше, изменяем его. */
  if (object != NULL)
    {
      hyscan_cached_remove_object_from_used (priv, object);
      object = hyscan_cached_update_object (priv, object, detail, data1, size1, data2, size2);
    }

  /* Если объекта в кэше не было, создаём новый и добавляем в кэш. */
  else
    {
      object = hyscan_cached_rise_object (priv, key, detail, data1, size1, data2, size2);
      g_hash_table_insert (priv->objects, &object->hash, object);
    }

  /* Перемещаем объект в начало списка используемых. */
  hyscan_cached_place_object_on_top_of_used (priv, object);

  g_rw_lock_writer_unlock (&priv->data_lock);

  return TRUE;
}

/* Функция считывает объект из кэша. */
static gboolean
hyscan_cached_get (HyScanCache  *cache,
                   guint64       key,
                   guint64       detail,
                   guint32       size1,
                   HyScanBuffer *buffer1,
                   HyScanBuffer *buffer2)
{
  HyScanCached *cached = HYSCAN_CACHED (cache);
  HyScanCachedPrivate *priv = cached->priv;

  ObjectInfo *object;
  guint32 size2 = 0;

  /* Проверка буферов. */
  if (buffer1 == NULL && buffer2 != NULL)
    return FALSE;

  g_rw_lock_reader_lock (&priv->data_lock);

  /* Ищем объект в кэше. */
  object = g_hash_table_lookup (priv->objects, &key);

  /* Объекта в кэше нет. */
  if (object == NULL)
    {
      g_rw_lock_reader_unlock (&priv->data_lock);
      return FALSE;
    }

  /* Не совпадает дополнительная информация. */
  if (detail != 0 && object->detail != detail)
    {
      g_rw_lock_reader_unlock (&priv->data_lock);
      return FALSE;
    }

  /* Перемещаем объект в начало списка используемых. */
  hyscan_cached_place_object_on_top_of_used (priv, object);

  /* Копируем первую часть данных объекта. */
  size1 = MIN (size1, object->size);
  if (buffer1 != NULL)
    {
      gpointer data;

      hyscan_buffer_set_size (buffer1, size1);
      data = hyscan_buffer_get_data (buffer1, &size1);
      memcpy (data, object->data, size1);
    }

  /* Копируем вторую часть данных объекта. */
  size2 = object->size - size1;
  if (buffer2 != NULL)
    {
      gpointer data;

      hyscan_buffer_set_size (buffer2, size2);
      data = hyscan_buffer_get_data (buffer2, &size2);
      memcpy (data, object->data + size1, size2);
    }

  g_rw_lock_reader_unlock (&priv->data_lock);

  return TRUE;
}

static void
hyscan_cached_interface_init (HyScanCacheInterface *iface)
{
  iface->set = hyscan_cached_set;
  iface->get = hyscan_cached_get;
}
