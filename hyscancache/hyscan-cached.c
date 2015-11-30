/*!
 * \file hyscan-cached.c
 *
 * \brief Исоходный файл системы кэширования данных в оперативной памяти
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 */

#include "hyscan-cached.h"
#include "hyscan-hash.h"

#include <string.h>
#include <stdlib.h>

#ifdef CPU_ARCH_X32
  #define MIN_CACHE_SIZE   64
  #define MAX_CACHE_SIZE   2048
#else
  #define MIN_CACHE_SIZE   64
  #define MAX_CACHE_SIZE   131072
#endif

#define SMALL_OBJECT_SIZE  sizeof (gint64)     /* Размер объекта для которого значения
                                                  сохраняются в метаданных объекта. */

enum
{
  PROP_O,
  PROP_CACHE_SIZE
};

/* Информация об объекте. */
typedef struct _ObjectInfo ObjectInfo;
struct _ObjectInfo
{
  guint64              hash;                   /* Хэш идентификатора объекта. */
  guint64              detail;                 /* Хэш дополнительной информации объекта. */
  gint32               size;                   /* Размер объекта. */
  gint32               allocated;              /* Размер выделенной для объекта области памяти. */

  union {
    gpointer          *data;                   /* Указатель на данные объекта. */
    gint64             value;                  /* Значение данных для объектов размером до 8 байт. */
  };

  ObjectInfo          *prev;                   /* Указатель на предыдущий объект. */
  ObjectInfo          *next;                   /* Указатель на следующий объект. */
};

/* Внутренние данные объекта. */
struct _HyScanCached
{
  GObject              parent_instance;

  gint64               cache_size;             /* Максимальный размер данных в кэше. */
  gint64               used_size;              /* Текущий размер данных в кэше. */

  gint64               max_n_objects;          /* Максимальное число объектов в кэше. */
  gint64               n_objects;              /* Текущее число объектов в кэше. */

  GHashTable          *objects;                /* Таблица объектов кэша. */
  GTrashStack         *free_objects;           /* "Куча" свободных структур с информацией об объекте. */

  ObjectInfo          *top_object;             /* Указатель на объект доступ к которому осуществлялся недавно. */
  ObjectInfo          *bottom_object;          /* Указатель на объект доступ к которому осуществлялся давно. */

  GRWLock              cache_lock;             /* Блокировка доступа к кэшу. */
  GRWLock              object_lock;            /* Блокировка доступа к объектам. */
};



static void            hyscan_cached_interface_init               (HyScanCacheInterface *iface);
static void            hyscan_cached_set_property                 (GObject              *object,
                                                                   guint                 prop_id,
                                                                   const GValue         *value,
                                                                   GParamSpec           *pspec);
static void            hyscan_cached_object_constructed           (GObject              *object);
static void            hyscan_cached_object_finalize              (GObject              *object);

static void            hyscan_cached_free_object                  (gpointer              data);
static void            hyscan_cached_free_used                    (HyScanCached         *cached,
                                                                   gint32                size);

static ObjectInfo     *hyscan_cached_rise_object                  (HyScanCached         *cache,
                                                                   guint64               key,
                                                                   guint64               detail,
                                                                   gpointer              data1,
                                                                   guint32               size1,
                                                                   gpointer              data2,
                                                                   guint32               size2);
static ObjectInfo     *hyscan_cached_update_object                (HyScanCached         *cache,
                                                                   ObjectInfo           *object,
                                                                   guint64               detail,
                                                                   gpointer              data1,
                                                                   guint32               size1,
                                                                   gpointer              data2,
                                                                   guint32               size2);
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
                                   g_param_spec_uint ("cache-size", "Cache size", "Cache size, Mb", 0, G_MAXUINT32, 256,
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
      if (cached->cache_size < MIN_CACHE_SIZE)
        cached->cache_size = MIN_CACHE_SIZE;
      if (cached->cache_size > MAX_CACHE_SIZE)
        cached->cache_size = MAX_CACHE_SIZE;
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

  gint64 i;

  /* Максимально возможное число объектов в кэше. */
  cached->max_n_objects = cached->cache_size / 8192;

  /* Выделяем память для структур с информацией об объектах. */
  for (i = 0; i < cached->max_n_objects; i++)
    {
      ObjectInfo *object = g_slice_new0( ObjectInfo );
      g_trash_stack_push (&cached->free_objects, object);
    }

  /* Таблица объектов кэша. */
  cached->objects = g_hash_table_new_full (g_int64_hash, g_int64_equal, NULL, hyscan_cached_free_object);

  /* Блокировки. */
  g_rw_lock_init (&cached->cache_lock);
  g_rw_lock_init (&cached->object_lock);
}

static void
hyscan_cached_object_finalize (GObject *object)
{
  HyScanCached *cached = HYSCAN_CACHED (object);

  ObjectInfo *cache_object;

  /* Блокировки. */
  g_rw_lock_clear (&cached->cache_lock);
  g_rw_lock_clear (&cached->object_lock);

  /* Освобождаем память структур используемых объектов. */
  g_hash_table_unref (cached->objects);

  /* Освобождаем память свободных структур с информацией об объекте. */
  while ((cache_object = g_trash_stack_pop (&cached->free_objects)))
    g_slice_free( ObjectInfo, cache_object);

  G_OBJECT_CLASS (hyscan_cached_parent_class)->finalize (object);
}

/* Функция освобождает память структуры используемого объекта. */
static void
hyscan_cached_free_object (gpointer data)
{
  ObjectInfo *object = data;
  if (object->allocated > 0)
    g_free( object->data );
  g_slice_free (ObjectInfo, object);
}

/* Функция освобождает память в кэше для размещения нового объекта. */
static void
hyscan_cached_free_used (HyScanCached *cached,
                         gint32        size)
{
  ObjectInfo *object = cached->bottom_object;

  gint32 can_free = 0;

  /* Удаляем объекты пока не наберём достаточного объёма свободной памяти. */
  while (object && (cached->cache_size - cached->used_size < size || cached->n_objects == cached->max_n_objects))
    {
      /* Если объем памяти "маленьких" объектов превысил два необходимых нам размера, удаляем их. */
      if (can_free > 2 * size)
        {
          object = object->next;
          while (object)
            {
              ObjectInfo *next_object = object->next;
              hyscan_cached_drop_object (cached, object);
              object = next_object;
            }
          break;
        }

      /* В первую очередь удаляем объекты размером не менее 20% от необходимого размера. */
      if (object->allocated >= size / 5)
        {
          hyscan_cached_drop_object (cached, object);
          object = cached->bottom_object;
        }
      /* Объекты меньшего размера не трогаем, но запоминаем сколько можно высвободить памяти. */
      else
        {
          can_free += object->allocated;
          object = object->prev;
        }
    }
}

/* Функция выбирает структуру с информацией об объекте из кучи свободных, выделяет память под объект
   и сохраняет данные. */
static ObjectInfo*
hyscan_cached_rise_object (HyScanCached *cached,
                           guint64       key,
                           guint64       detail,
                           gpointer      data1,
                           guint32       size1,
                           gpointer      data2,
                           guint32       size2)
{
  ObjectInfo *object = g_trash_stack_pop (&cached->free_objects);

  gint32 size = size1 + size2;

  if (object == NULL)
    return NULL;

  /* Инициализация. */
  object->next = NULL;
  object->prev = NULL;

  /* Хеш идентификатора объекта и дополнительной информации. */
  object->hash = key;
  object->detail = detail;
  object->size = size;

  /* Данные объекта большого размера. */
  if (size > SMALL_OBJECT_SIZE)
    {
      object->data = g_malloc (size);
      object->allocated = size;
      cached->used_size += size;
      memcpy (object->data, data1, size1);
      if (size2 > 0)
        memcpy ((gint8*) object->data + size1, data2, size2);
    }

  /* Данные объекта малого размера. */
  else
    {
      object->allocated = 0;
      object->value = 0;
      memcpy (&object->value, data1, size1);
      if (size2 > 0)
        memcpy (((gint8*) &object->value) + size1, data2, size2);
    }

  cached->n_objects += 1;

  return object;
}

/* Функция обновляет используемый объект. */
static ObjectInfo*
hyscan_cached_update_object (HyScanCached *cached,
                             ObjectInfo   *object,
                             guint64       detail,
                             gpointer      data1,
                             guint32       size1,
                             gpointer      data2,
                             guint32       size2)
{
  gint32 size = size1 + size2;

  /* Если текущий размер объекта меньше нового размера или больше нового на 5%, выделяем память заново. */
  if (size <= SMALL_OBJECT_SIZE || object->allocated < size
      || ((gdouble) size / (gdouble) object->allocated) < 0.95)
    {
      if (object->allocated > 0)
        {
          g_free (object->data);
          cached->used_size -= object->allocated;
        }
      if (size > SMALL_OBJECT_SIZE)
        {
          object->data = g_malloc (size);
          object->allocated = size;
          cached->used_size += size;
        }
      else
        {
        object->value = 0;
        }
    }

  /* Новый размер объекта. */
  object->size = size;
  object->detail = detail;

  /* Данные объекта. */
  if (size > SMALL_OBJECT_SIZE)
    {
      memcpy (object->data, data1, size1);
      if (size2 > 0)
        memcpy ((gint8*) object->data + size1, data2, size2);
    }
  else
    {
      memcpy (&object->value, data1, size1);
      if (size2 > 0)
        memcpy (((gint8*) &object->value) + size1, data2, size2);
    }

  return object;
}

/* Функция удаляет объект из кеша и помещает структуру в кучу свободных. */
static void
hyscan_cached_drop_object (HyScanCached *cached,
                           ObjectInfo   *object)
{
  hyscan_cached_remove_object_from_used (cached, object);

  cached->n_objects -= 1;
  if (object->allocated)
    {
      g_free (object->data);
      cached->used_size -= object->allocated;
    }

  g_hash_table_steal (cached->objects, &object->hash);
  g_trash_stack_push (&cached->free_objects, object);
}

/* Функция удаляет объект из списка используемых. */
static void
hyscan_cached_remove_object_from_used (HyScanCached *cached,
                                       ObjectInfo   *object)
{
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
  g_rw_lock_writer_lock (&cached->object_lock);

  /* Первый объект в кэше. */
  if (cached->top_object == NULL && cached->bottom_object == NULL)
    {
      cached->top_object = object;
      cached->bottom_object = object;
      g_rw_lock_writer_unlock (&cached->object_lock);
      return;
    }

  /* "Вынимаем" объект из цепочки используемых. */
  hyscan_cached_remove_object_from_used (cached, object);

  /* "Вставляем" перед первым объектом в цепочке. */
  if (cached->top_object != NULL)
    {
      cached->top_object->prev = object;
      object->next = cached->top_object;
    }

  /* Изменяем указатель начала цепочки. */
  cached->top_object = object;

  g_rw_lock_writer_unlock (&cached->object_lock);
}


/* Функция добавляет или изменяет объект в кэше. */
gboolean
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

  /* Если размер нового объекта слишком большой, не сохраняем его. */
  if (size > cached->cache_size / 10)
    return FALSE;

  /* Проверяем размер данных. */
  if (size1 < 0 || size2 < 0)
    return FALSE;

  g_rw_lock_writer_lock (&cached->cache_lock);

  /* Ищем объект в кэше. */
  object = g_hash_table_lookup (cached->objects, &key);

  /* Если размер объекта равен нулю или нет данных, удаляем объект. */
  if (size == 0 || data1 == NULL)
    {
      if (object != NULL)
        hyscan_cached_drop_object (cached, object);
      g_rw_lock_writer_unlock (&cached->cache_lock);
      return TRUE;
    }

  /* Очищаем кэш если ... */

  /* ... достигнут лимит используемой памяти. */
  if ((size > SMALL_OBJECT_SIZE) && (cached->used_size + size > cached->cache_size))
    hyscan_cached_free_used (cached, size);

  /* ... достигнут лимит числа объектов. */
  if (object == NULL && cached->n_objects == cached->max_n_objects)
    hyscan_cached_free_used (cached, 0);

  /* Если объект уже был в кэше, изменяем его. */
  if (object != NULL)
    {
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

  g_rw_lock_writer_unlock (&cached->cache_lock);

  return TRUE;
}

/* Функция добавляет или изменяет объект в кэше. */
gboolean
hyscan_cached_set (HyScanCache *cache,
                   guint64      key,
                   guint64      detail,
                   gpointer     data,
                   gint32       size)
{
  return hyscan_cached_set2 (cache, key, detail, data, size, NULL, 0);
}

/* Функция считывает объект из кэша. */
gboolean
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

  /* Проверка размеров буферов. */
  if (buffer1 != NULL && *buffer1_size <= 0)
    return FALSE;
  if (buffer2 != NULL && *buffer2_size <= 0)
    return FALSE;

  g_rw_lock_reader_lock (&cached->cache_lock);

  /* Ищем объект в кэше. */
  object = g_hash_table_lookup (cached->objects, &key);

  /* Объекта в кэше нет. */
  if (object == NULL)
    {
      g_rw_lock_reader_unlock (&cached->cache_lock);
      return FALSE;
    }

  /* Не совпадает дополнительная информация. */
  if (detail != 0 && object->detail != detail)
    {
      g_rw_lock_reader_unlock (&cached->cache_lock);
      return FALSE;
    }

  /* Перемещаем объект в начало списка используемых. */
  hyscan_cached_place_object_on_top_of_used (cached, object);

  /* Копируем первую часть данных объекта. */
  if (buffer1 != NULL)
    {
      *buffer1_size = *buffer1_size < object->size ? *buffer1_size : object->size;
      if (object->size > SMALL_OBJECT_SIZE)
        memcpy (buffer1, object->data, *buffer1_size);
      else
        memcpy (buffer1, &object->value, *buffer1_size);
    }

  /* Копируем вторую часть данных объекта. */
  if (buffer1 != NULL && buffer2 != NULL)
    {
      *buffer2_size = *buffer2_size < (object->size - *buffer1_size) ?
                        *buffer2_size : (object->size - *buffer1_size);
      if (object->size > SMALL_OBJECT_SIZE)
        memcpy (buffer2, (gint8*) object->data + *buffer1_size, *buffer2_size);
      else
        memcpy (buffer2, ((gint8*) &object->value) + *buffer1_size, *buffer2_size);
    }

  /* Размер объекта в кэше. */
  if (buffer1 == NULL && buffer2 == NULL)
    {
      if (buffer1_size != NULL)
        *buffer1_size = object->size;
      if (buffer2_size != NULL)
        *buffer2_size = 0;
    }

  g_rw_lock_reader_unlock (&cached->cache_lock);

  return TRUE;
}

/* Функция считывает объект из кэша. */
gboolean
hyscan_cached_get (HyScanCache *cache,
                   guint64      key,
                   guint64      detail,
                   gpointer     buffer,
                   gint32      *buffer_size)
{
  return hyscan_cached_get2 (cache, key,detail, buffer, buffer_size, NULL, NULL);
}

/* Функция создаёт новый объект HyScanCached. */
HyScanCached*
hyscan_cached_new (guint32 cache_size)
{
  return g_object_new (HYSCAN_TYPE_CACHED, "cache-size", cache_size, NULL);
}

static void
hyscan_cached_interface_init (HyScanCacheInterface *iface)
{
  iface->set = hyscan_cached_set;
  iface->set2 = hyscan_cached_set2;
  iface->get = hyscan_cached_get;
  iface->get2 = hyscan_cached_get2;
}
