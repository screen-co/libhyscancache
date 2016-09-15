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

  gint32               allocated;              /* Размер буфера. */
  gint32               size;                   /* Размер объекта. */
  gint8                data[];                 /* Данные объекта. */
};

/* Внутренние данные объекта. */
struct _HyScanCached
{
  GObject              parent_instance;

  gint64               cache_size;             /* Максимальный размер данных в кэше. */
  gint64               used_size;              /* Текущий размер данных в кэше. */

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

static void            hyscan_cached_free_used                    (HyScanCached         *cached,
                                                                   gint32                size);

static ObjectInfo     *hyscan_cached_rise_object                  (HyScanCached         *cache,
                                                                   guint64               key,
                                                                   guint64               detail,
                                                                   gpointer              data1,
                                                                   gint32                size1,
                                                                   gpointer              data2,
                                                                   gint32                size2);
static ObjectInfo     *hyscan_cached_update_object                (HyScanCached         *cache,
                                                                   ObjectInfo           *object,
                                                                   guint64               detail,
                                                                   gpointer              data1,
                                                                   gint32                size1,
                                                                   gpointer              data2,
                                                                   gint32                size2);
static void            hyscan_cached_drop_object                  (HyScanCached         *cache,
                                                                   ObjectInfo           *object);

static void            hyscan_cached_remove_object_from_used      (HyScanCached         *cache,
                                                                   ObjectInfo           *object);
static void            hyscan_cached_place_object_on_top_of_used  (HyScanCached         *cache,
                                                                   ObjectInfo           *object);

G_DEFINE_TYPE_WITH_CODE (HyScanCached, hyscan_cached, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (HYSCAN_TYPE_CACHE, hyscan_cached_interface_init));

static void
hyscan_cached_class_init (HyScanCachedClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS( klass );

  object_class->set_property = hyscan_cached_set_property;
  object_class->constructed = hyscan_cached_object_constructed;
  object_class->finalize = hyscan_cached_object_finalize;

  g_object_class_install_property (object_class, PROP_CACHE_SIZE,
    g_param_spec_uint ("cache-size", "Cache size", "Cache size, Mb",
                       MIN_CACHE_SIZE, MAX_CACHE_SIZE, MIN_CACHE_SIZE,
                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
hyscan_cached_init (HyScanCached *cache)
{
}

static void
hyscan_cached_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  HyScanCached *cached = HYSCAN_CACHED (object);

  switch (prop_id)
    {
    case PROP_CACHE_SIZE:
      cached->cache_size = g_value_get_uint (value);
      cached->cache_size *= 1024 * 1024;
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

  g_rw_lock_init (&cached->data_lock);
  g_rw_lock_init (&cached->list_lock);

  /* Таблица объектов кэша. */
  cached->objects = g_hash_table_new_full (g_int64_hash, g_int64_equal, NULL, g_free);
}

static void
hyscan_cached_object_finalize (GObject *object)
{
  HyScanCached *cached = HYSCAN_CACHED (object);

  g_hash_table_unref (cached->objects);

  g_rw_lock_clear (&cached->list_lock);
  g_rw_lock_clear (&cached->data_lock);

  G_OBJECT_CLASS (hyscan_cached_parent_class)->finalize (object);
}

/* Функция освобождает память в кэше для размещения нового объекта. */
static void
hyscan_cached_free_used (HyScanCached *cached,
                         gint32        size)
{
  ObjectInfo *object = cached->bottom_object;

  /* Удаляем объекты пока не наберём достаточного объёма свободной памяти. */
  while (object != NULL && cached->cache_size < (cached->used_size + size))
    {
      hyscan_cached_drop_object (cached, object);
      object = cached->bottom_object;
    }
}

/* Функция выбирает структуру с информацией об объекте из кучи свободных, выделяет память под объект
   и сохраняет данные. */
static ObjectInfo *
hyscan_cached_rise_object (HyScanCached *cached,
                           guint64       key,
                           guint64       detail,
                           gpointer      data1,
                           gint32        size1,
                           gpointer      data2,
                           gint32        size2)
{
  ObjectInfo *object;
  gint32 size = size1 + size2;

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
  cached->used_size += (OBJECT_HEADER_SIZE + size);
  memcpy (object->data, data1, size1);
  if (size2 > 0)
    memcpy ((gint8*) object->data + size1, data2, size2);

  return object;
}

/* Функция обновляет используемый объект. */
static ObjectInfo *
hyscan_cached_update_object (HyScanCached *cached,
                             ObjectInfo   *object,
                             guint64       detail,
                             gpointer      data1,
                             gint32        size1,
                             gpointer      data2,
                             gint32        size2)
{
  gint32 size = size1 + size2;

  /* Если текущий размер объекта меньше нового размера или больше нового на 5%, выделяем память заново. */
  if (object->allocated < size || ((gdouble) size / (gdouble) object->allocated) < 0.95)
    {
      g_hash_table_steal (cached->objects, &object->hash);
      object = g_realloc (object, OBJECT_HEADER_SIZE + size);
      g_hash_table_insert (cached->objects, &object->hash, object);

      cached->used_size -= (OBJECT_HEADER_SIZE + object->allocated);
      object->allocated = size;
      cached->used_size += (OBJECT_HEADER_SIZE + size);
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
hyscan_cached_drop_object (HyScanCached *cached,
                           ObjectInfo   *object)
{
  hyscan_cached_remove_object_from_used (cached, object);

  cached->used_size -= (OBJECT_HEADER_SIZE + object->allocated);
  g_hash_table_remove (cached->objects, &object->hash);
}

/* Функция удаляет объект из списка используемых. */
static void
hyscan_cached_remove_object_from_used (HyScanCached *cached,
                                       ObjectInfo   *object)
{
  /* Единственный объект в списке */
  if ((cached->top_object == cached->bottom_object) && (cached->top_object == object))
    {
      cached->top_object = NULL;
      cached->bottom_object = NULL;
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
      cached->top_object = object->next;
      object->next->prev = NULL;
      object->next = NULL;
    }

  /* Последний объект в списке. */
  else
    {
      cached->bottom_object = object->prev;
      object->prev->next = NULL;
      object->prev = NULL;
    }
}

/* Функция перемещает объект на вершину списка часто используемых. */
static void
hyscan_cached_place_object_on_top_of_used (HyScanCached *cached,
                                           ObjectInfo   *object)
{
  g_rw_lock_writer_lock (&cached->list_lock);

  /* "Вынимаем" объект из цепочки используемых. */
  hyscan_cached_remove_object_from_used (cached, object);

  /* Первый объект в кэше. */
  if ((cached->top_object == NULL) && (cached->bottom_object == NULL))
    {
      cached->top_object = object;
      cached->bottom_object = object;
    }

  /* "Вставляем" перед первым объектом в цепочке. */
  else
    {
      cached->top_object->prev = object;
      object->next = cached->top_object;
      cached->top_object = object;
    }

  g_rw_lock_writer_unlock (&cached->list_lock);
}

/* Функция создаёт новый объект HyScanCached. */
HyScanCached *
hyscan_cached_new (guint32 cache_size)
{
  return g_object_new (HYSCAN_TYPE_CACHED, "cache-size", cache_size, NULL);
}

/* Функция добавляет или изменяет объект в кэше. */
static gboolean
hyscan_cached_set2 (HyScanCache *cache,
                    guint64      key,
                    guint64      detail,
                    gpointer     data1,
                    gint32       size1,
                    gpointer     data2,
                    gint32       size2)
{
  HyScanCached *cached = HYSCAN_CACHED( cache );

  gint32 size = size1 + size2;
  ObjectInfo *object;

  if (size1 < 0 || size2 < 0)
    return FALSE;

  /* Если размер нового объекта слишком большой, не сохраняем его. */
  if (size > cached->cache_size / 10)
    return FALSE;

  g_rw_lock_writer_lock (&cached->data_lock);

  /* Ищем объект в кэше. */
  object = g_hash_table_lookup (cached->objects, &key);

  /* Если размер объекта равен нулю или нет данных, удаляем объект. */
  if (size == 0 || data1 == NULL)
    {
      if (object != NULL)
        hyscan_cached_drop_object (cached, object);
      g_rw_lock_writer_unlock (&cached->data_lock);
      return TRUE;
    }

  /* Очищаем кэш если достигнут лимит используемой памяти. */
  if (cached->used_size + OBJECT_HEADER_SIZE + size > cached->cache_size)
    {
      hyscan_cached_free_used (cached, OBJECT_HEADER_SIZE + size);
      object = g_hash_table_lookup (cached->objects, &key);
    }

  /* Если объект уже был в кэше, изменяем его. */
  if (object != NULL)
    {
      hyscan_cached_remove_object_from_used (cached, object);
      object = hyscan_cached_update_object (cached, object, detail, data1, size1, data2, size2);
    }

  /* Если объекта в кэше не было, создаём новый и добавляем в кэш. */
  else
    {
      object = hyscan_cached_rise_object (cached, key, detail, data1, size1, data2, size2);
      g_hash_table_insert (cached->objects, &object->hash, object);
    }

  /* Перемещаем объект в начало списка используемых. */
  hyscan_cached_place_object_on_top_of_used (cached, object);

  g_rw_lock_writer_unlock (&cached->data_lock);

  return TRUE;
}

/* Функция добавляет или изменяет объект в кэше. */
static gboolean
hyscan_cached_set (HyScanCache *cache,
                   guint64      key,
                   guint64      detail,
                   gpointer     data,
                   gint32       size)
{
  return hyscan_cached_set2 (cache, key, detail, data, size, NULL, 0);
}

/* Функция считывает объект из кэша. */
static gboolean
hyscan_cached_get2 (HyScanCache *cache,
                    guint64      key,
                    guint64      detail,
                    gpointer     buffer1,
                    gint32      *buffer1_size,
                    gpointer     buffer2,
                    gint32      *buffer2_size)
{
  HyScanCached *cached = HYSCAN_CACHED (cache);

  ObjectInfo *object;

  /* Проверка буферов. */
  if (buffer1 != NULL && *buffer1_size <= 0)
    return FALSE;
  if (buffer2 != NULL && *buffer2_size <= 0)
    return FALSE;
  if (buffer1 == NULL && buffer2 != NULL)
    return FALSE;

  g_rw_lock_reader_lock (&cached->data_lock);

  /* Ищем объект в кэше. */
  object = g_hash_table_lookup (cached->objects, &key);

  /* Объекта в кэше нет. */
  if (object == NULL)
    {
      g_rw_lock_reader_unlock (&cached->data_lock);
      return FALSE;
    }

  /* Не совпадает дополнительная информация. */
  if (detail != 0 && object->detail != detail)
    {
      g_rw_lock_reader_unlock (&cached->data_lock);
      return FALSE;
    }

  /* Перемещаем объект в начало списка используемых. */
  hyscan_cached_place_object_on_top_of_used (cached, object);

  /* Копируем первую часть данных объекта. */
  if (buffer1 != NULL)
    {
      *buffer1_size = *buffer1_size < object->size ? *buffer1_size : object->size;
      memcpy (buffer1, object->data, *buffer1_size);
    }

  /* Копируем вторую часть данных объекта. */
  if (buffer2 != NULL)
    {
      *buffer2_size = *buffer2_size < (object->size - *buffer1_size) ?
                        *buffer2_size : (object->size - *buffer1_size);
      memcpy (buffer2, (gint8*) object->data + *buffer1_size, *buffer2_size);
    }

  /* Размер объекта в кэше. */
  if (buffer1 == NULL && buffer2 == NULL)
    {
      if (buffer1_size != NULL)
        *buffer1_size = object->size;
      if (buffer2_size != NULL)
        *buffer2_size = 0;
    }

  g_rw_lock_reader_unlock (&cached->data_lock);

  return TRUE;
}

/* Функция считывает объект из кэша. */
static gboolean
hyscan_cached_get (HyScanCache *cache,
                   guint64      key,
                   guint64      detail,
                   gpointer     buffer,
                   gint32      *buffer_size)
{
  return hyscan_cached_get2 (cache, key,detail, buffer, buffer_size, NULL, NULL);
}

static void
hyscan_cached_interface_init (HyScanCacheInterface *iface)
{
  iface->set = hyscan_cached_set;
  iface->set2 = hyscan_cached_set2;
  iface->get = hyscan_cached_get;
  iface->get2 = hyscan_cached_get2;
}
