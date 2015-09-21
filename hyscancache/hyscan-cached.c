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
  #define MIN_CACHE_SIZE 64
  #define MAX_CACHE_SIZE 2048
#else
  #define MIN_CACHE_SIZE 64
  #define MAX_CACHE_SIZE 131072
#endif


enum { PROP_O, PROP_CACHE_SIZE };


typedef struct ObjectInfo ObjectInfo;
struct ObjectInfo {                              // Информация об объекте.

  guint64                    hash;               // Хэш идентификатора объекта.
  guint64                    detail;             // Хэш дополнительной информации объекта.
  guint32                    size;               // Размер объекта.
  guint32                    allocated;          // Размер выделенной для объекта области памяти.
  gpointer                  *data;               // Указатель на данные объекта.

  ObjectInfo                *prev;               // Указатель на предыдущий объект.
  ObjectInfo                *next;               // Указатель на следующий объект.

};


typedef struct HyScanCachedPriv {                // Внутренние данные объекта.

  gint64                     cache_size;         // Максимальный размер данных в кэше.
  gint64                     used_size;          // Текущий размер данных в кэше.

  gint64                     max_n_objects;      // Максимальное число объектов в кэше.
  gint64                     n_objects;          // Текущее число объектов в кэше.

  GHashTable                *objects;            // Таблица объектов кэша.
  GTrashStack               *free_objects;       // "Куча" свободных структур с информацией об объекте.

  ObjectInfo                *top_object;         // Указатель на объект доступ к которому осуществлялся недавно.
  ObjectInfo                *bottom_object;      // Указатель на объект доступ к которому осуществлялся давно.

  GRWLock                    cache_lock;         // Блокировка доступа к кэшу.
  GRWLock                    object_lock;        // Блокировка доступа к объектам.

} HyScanCachedPriv;


#define HYSCAN_CACHED_GET_PRIVATE( obj ) ( G_TYPE_INSTANCE_GET_PRIVATE( ( obj ), HYSCAN_TYPE_CACHED, HyScanCachedPriv ) )


static void hyscan_cached_interface_init( HyScanCacheInterface *iface );
static void hyscan_cached_set_property( GObject *cache, guint prop_id, const GValue *value, GParamSpec *pspec );
static GObject* hyscan_cached_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties );
static void hyscan_cached_object_finalize( GObject *cache );

static void hyscan_cached_free_object( gpointer data );
static void hyscan_cached_free_used( HyScanCachedPriv *priv, gint32 size );

static ObjectInfo *hyscan_cached_rise_object( HyScanCachedPriv *priv, guint64 key, guint64 detail, gpointer data, guint32 size );
static ObjectInfo *hyscan_cached_update_object( HyScanCachedPriv *priv, ObjectInfo *object, guint64 detail, gpointer data, guint32 size );
static void hyscan_cached_drop_object( HyScanCachedPriv *priv, ObjectInfo *object );

static void hyscan_cached_remove_object_from_used( HyScanCachedPriv *priv, ObjectInfo *object );
static void hyscan_cached_place_object_on_top_of_used( HyScanCachedPriv *priv, ObjectInfo *object );


G_DEFINE_TYPE_WITH_CODE( HyScanCached, hyscan_cached, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE( HYSCAN_TYPE_CACHE, hyscan_cached_interface_init ) );


static void hyscan_cached_class_init( HyScanCachedClass *klass )
{

  GObjectClass *this_class = G_OBJECT_CLASS( klass );

  this_class->set_property = hyscan_cached_set_property;
  this_class->constructor = hyscan_cached_object_constructor;
  this_class->finalize = hyscan_cached_object_finalize;

  g_object_class_install_property( this_class, PROP_CACHE_SIZE,
    g_param_spec_uint( "cache-size", "Cache size", "Cache size, Mb", 0, G_MAXUINT32, 256, G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY ) );

  g_type_class_add_private( klass, sizeof( HyScanCachedPriv ) );

}


static void hyscan_cached_init( HyScanCached *cache ){ ; }


static void hyscan_cached_set_property( GObject *cache, guint prop_id, const GValue *value, GParamSpec *pspec )
{

  HyScanCachedPriv *priv = HYSCAN_CACHED_GET_PRIVATE( cache );

  switch ( prop_id )
    {

    case PROP_CACHE_SIZE:
      priv->cache_size = g_value_get_uint( value );
      if( priv->cache_size < MIN_CACHE_SIZE ) priv->cache_size = MIN_CACHE_SIZE;
      if( priv->cache_size > MAX_CACHE_SIZE ) priv->cache_size = MAX_CACHE_SIZE;
      priv->cache_size *= 1024 * 1024;
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID( cache, prop_id, pspec );
      break;

    }

}


static GObject* hyscan_cached_object_constructor( GType g_type, guint n_construct_properties, GObjectConstructParam *construct_properties )
{

  GObject *cache = G_OBJECT_CLASS( hyscan_cached_parent_class )->constructor( g_type, n_construct_properties, construct_properties );
  HyScanCachedPriv *priv = HYSCAN_CACHED_GET_PRIVATE( cache );

  guint64 i;

  // Максимально возможное число объектов в кэше.
  priv->max_n_objects = priv->cache_size / 8192;

  // Выделяем память для структур с информацией об объектах.
  for( i = 0; i < priv->max_n_objects; i++ )
    {
    ObjectInfo *object = g_slice_new0( ObjectInfo );
    g_trash_stack_push( &priv->free_objects, object );
    }

  // Таблица объектов кэша.
  priv->objects = g_hash_table_new_full( g_int64_hash, g_int64_equal, NULL, hyscan_cached_free_object );

  // Блокировки.
  g_rw_lock_init( &priv->cache_lock );
  g_rw_lock_init( &priv->object_lock );

  return cache;

}


static void hyscan_cached_object_finalize( GObject *cache )
{

  HyScanCachedPriv *priv = HYSCAN_CACHED_GET_PRIVATE( cache );

  ObjectInfo *object;

  // Блокировки.
  g_rw_lock_clear( &priv->cache_lock );
  g_rw_lock_clear( &priv->object_lock );

  // Освобождаем память структур используемых объектов.
  g_hash_table_unref( priv->objects );

  // Освобождаем память свободных структур с информацией об объекте.
  while( ( object = g_trash_stack_pop( &priv->free_objects ) ) ) g_slice_free( ObjectInfo, object );

  G_OBJECT_CLASS( hyscan_cached_parent_class )->finalize( cache );

}


// Функция освобождает память структуры используемого объекта.
static void hyscan_cached_free_object( gpointer data )
{

  ObjectInfo *object = data;

  free( object->data );
  g_slice_free( ObjectInfo, object );

}


// Функция освобождает память в кэше для размещения нового объекта.
static void hyscan_cached_free_used( HyScanCachedPriv *priv, gint32 size )
{

  ObjectInfo *object = priv->bottom_object;

  gint32 can_free = 0;

  // Удаляем объекты пока не наберём достаточного объёма свободной памяти.
  while( object && ( priv->cache_size - priv->used_size < size || priv->n_objects == priv->max_n_objects ) )
    {

    // Если объем памяти "маленьких" объектов превысил два необходимых нам размера, удаляем их.
    if( can_free > 2 * size )
      {
      object = object->next;
      while( object )
        {
        ObjectInfo *next_object = object->next;
        hyscan_cached_drop_object( priv, object );
        object = next_object;
        }
      break;
      }

    // В первую очередь удаляем объекты размером не менее 20% от необходимого размера.
    if( object->allocated > size / 5 )
      {
      hyscan_cached_drop_object( priv, object );
      object = priv->bottom_object;
      }

    // Объекты меньшего размера не трогаем, но запоминаем сколько можно высвободить памяти.
    else
      {
      can_free += object->allocated;
      object = object->prev;
      }

    }

}


// Функция выбирает структуру с информацией об объекте из кучи свободных, выделяет память под объект
static ObjectInfo *hyscan_cached_rise_object( HyScanCachedPriv *priv, guint64 key, guint64 detail, gpointer data, guint32 size )
{

  ObjectInfo *object = g_trash_stack_pop( &priv->free_objects );

  if( !object ) return NULL;

  // Инициализация.
  object->next = NULL;
  object->prev = NULL;

  // Хеш идентификатора объекта и дополнительной информации.
  object->hash =  key;
  object->detail =  detail;

  // Данные объекта.
  object->size = size;
  object->allocated = size;
  object->data = malloc( size );
  if( object->data )
    {
    memcpy( object->data, data, size );
    priv->used_size += size;
    priv->n_objects += 1;
    }
  else
    {
    g_trash_stack_push( &priv->free_objects, object );
    object = NULL;
    }

  return object;

}


// Функция обновляет используемый объект.
static ObjectInfo *hyscan_cached_update_object( HyScanCachedPriv *priv, ObjectInfo *object, guint64 detail, gpointer data, guint32 size )
{


  // Если текущий размер объекта меньше нового размера или больше нового на 5%, выделяем память заново.
  if( object->size < size || ( (gdouble)size / (gdouble)object->allocated ) < 0.95 )
    {
    priv->used_size -= object->allocated;
    free( object->data );
    object->data = malloc( size );
    object->allocated = size;
    priv->used_size += size;
    }

  // Новый размер объекта.
  object->size = size;
  object->detail = detail;

  // Данные объекта.
  if( object->data ) memcpy( object->data, data, size );
  else
    {
    hyscan_cached_drop_object( priv, object );
    object = NULL;
    }

  return object;

}


// Функция удаляет объект из кеша и помещает структуру в кучу свободных.
static void hyscan_cached_drop_object( HyScanCachedPriv *priv, ObjectInfo *object )
{

  hyscan_cached_remove_object_from_used( priv, object );

  priv->n_objects -= 1;
  priv->used_size -= object->allocated;
  free( object->data );

  g_hash_table_steal( priv->objects, &object->hash );
  g_trash_stack_push( &priv->free_objects, object );

}


// Функция удаляет объект из списка используемых.
static void hyscan_cached_remove_object_from_used( HyScanCachedPriv *priv, ObjectInfo *object )
{

  // Объект не в списке.
  if( !object->prev && !object->next ) return;

  // Объект в списке.
  if( object->prev && object->next )
    {
    object->prev->next = object->next;
    object->next->prev = object->prev;
    object->prev = NULL;
    object->next = NULL;
    }

  // Первый объект в списке.
  else if( !object->prev )
    {
    priv->top_object = object->next;
    object->next->prev = NULL;
    object->next = NULL;
    }

  // Последний объект в списке.
  else
    {
    priv->bottom_object = object->prev;
    object->prev->next = NULL;
    object->prev = NULL;
    }

}


// Функция перемещает объект на вершину списка часто используемых.
static void hyscan_cached_place_object_on_top_of_used( HyScanCachedPriv *priv, ObjectInfo *object )
{

  g_rw_lock_writer_lock( &priv->object_lock );

  // Первый объект в кэше.
  if( !priv->top_object && !priv->bottom_object )
    {
    priv->top_object = object;
    priv->bottom_object = object;
    g_rw_lock_writer_unlock( &priv->object_lock );
    return;
    }

  // "Вынимаем" объект из цепочки используемых.
  hyscan_cached_remove_object_from_used( priv, object );

  // "Вставляем" перед первым объектом в цепочке.
  if( priv->top_object )
    {
    priv->top_object->prev = object;
    object->next = priv->top_object;
    }

  // Изменяем указатель начала цепочки.
  priv->top_object = object;

  g_rw_lock_writer_unlock( &priv->object_lock );

}


// Функция добавляет или изменяет объект в кэше.
gboolean hyscan_cached_set( HyScanCache *cache, guint64 key, guint64 detail, gpointer data, guint32 size )
{

  HyScanCachedPriv *priv = HYSCAN_CACHED_GET_PRIVATE( cache );

  ObjectInfo *object;

  // Если размер нового объекта слишком большой, не сохраняем его.
  if( size > priv->cache_size / 10 ) return FALSE;

  g_rw_lock_writer_lock( &priv->cache_lock );

  // Очищаем кэш если ...

  // ... достигнут лимит используемой памяти.
  if( priv->used_size + size > priv->cache_size )
    hyscan_cached_free_used( priv, size );

  // ... достигнут лимит числа объектов.
  if( priv->n_objects == priv->max_n_objects )
    hyscan_cached_free_used( priv, 1 );

  // Ищем объект в кэше.
  object = g_hash_table_lookup( priv->objects, &key );

  // Если объект уже был в кэше, изменяем его.
  if( object )
    object = hyscan_cached_update_object( priv, object, detail, data, size );

  // Если объекта в кэше не было, создаём новый.
  else
    object = hyscan_cached_rise_object( priv, key, detail, data, size );

  // Произошла ошибка при выделении памяти для объекта.
  if( !object )
    { g_rw_lock_writer_unlock( &priv->cache_lock ); return FALSE; }

  // Добавляем объект в кэш.
  if( !g_hash_table_contains( priv->objects, &object->hash ) )
    g_hash_table_insert( priv->objects, &object->hash, object );

  // Перемещаем объект в начало списка используемых.
  hyscan_cached_place_object_on_top_of_used( priv, object );

  g_rw_lock_writer_unlock( &priv->cache_lock );

  return TRUE;

}


// Функция считывает объект из кэша.
gboolean hyscan_cached_get( HyScanCache *cache, guint64 key, guint64 detail, gpointer buffer, guint32 *buffer_size )
{

  HyScanCachedPriv *priv = HYSCAN_CACHED_GET_PRIVATE( cache );

  ObjectInfo *object;

  g_rw_lock_reader_lock( &priv->cache_lock );

  // Ищем объект в кэше.
  object= g_hash_table_lookup( priv->objects, &key );

  // Объекта в кэше нет.
  if( !object )
    { g_rw_lock_reader_unlock( &priv->cache_lock ); return FALSE; }

  // Не совпадает дополнительная информация.
  if( detail && object->detail != detail )
    { g_rw_lock_reader_unlock( &priv->cache_lock ); return FALSE; }

  // Перемещаем объект в начало списка используемых.
  hyscan_cached_place_object_on_top_of_used( priv, object );

  // Копируем данные объекта.
  if( buffer )
    {
    *buffer_size = *buffer_size < object->size ? *buffer_size : object->size;
    memcpy( buffer, object->data, *buffer_size );
    }
  else
    *buffer_size = object->size;

  g_rw_lock_reader_unlock( &priv->cache_lock );

  return TRUE;

}


// Функция создаёт новый объект HyScanCached.
HyScanCached *hyscan_cached_new( guint32 cache_size )
{

  return g_object_new( HYSCAN_TYPE_CACHED, "cache-size", cache_size, NULL );

}


static void hyscan_cached_interface_init( HyScanCacheInterface *iface )
{

  iface->set = hyscan_cached_set;
  iface->get = hyscan_cached_get;

}
