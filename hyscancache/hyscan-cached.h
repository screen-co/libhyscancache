/*!
 * \file hyscan-cached.h
 *
 * \brief Заголовочный файл системы кэширования данных в оперативной памяти
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanCached HyScanCached - реализация системы кэширования данных в оперативной памяти
 *
 * HyScanCached реализация интерфейса \link HyScanCache \endlink основанная на хранении данных в
 * оперативной памяти. Объект HyScanCached создаётся при помощи функции #hyscan_cached_new. При
 * создании объекта указывается максимальный объём памяти, используемый для хранения объектов
 * в системе кэширования. Из-за особенностей реализации алгоритмов выделения памяти, объект
 * HyScanCached может использовать на 5-10% больше памяти, чем указано пользователем.
 *
 * HyScanCached может хранить ограниченное число объектов. Число объектов зависит от объёма
 * памяти выделенной для кэша и равно CACHE_SIZE / 8192, где CACHE_SIZE - максимальный размер
 * кэша в байтах.
 *
 * В случае исчерпания всего доступного объёма памяти или превышения лимита объектов, HyScanCached
 * удаляет объекты, доступ к которым осуществлялся давно, и использует освободившуюся память для
 * сохранения новых объектов.
 *
*/

#ifndef _hyscan_cached_h
#define _hyscan_cached_h

#include <hyscan-cache.h>

G_BEGIN_DECLS


#define HYSCAN_TYPE_CACHED                      ( hyscan_cached_get_type() )
#define HYSCAN_CACHED( obj )                    ( G_TYPE_CHECK_INSTANCE_CAST( ( obj ), HYSCAN_TYPE_CACHED, HyScanCached ) )
#define HYSCAN_IS_CACHED( obj )                 ( G_TYPE_CHECK_INSTANCE_TYPE( ( obj ), HYSCAN_TYPE_CACHED ) )
#define HYSCAN_CACHED_CLASS( klass )            ( G_TYPE_CHECK_CLASS_CAST( ( klass ), HYSCAN_TYPE_CACHED, HyScanCachedClass ) )
#define HYSCAN_IS_CACHED_CLASS( klass )         ( G_TYPE_CHECK_CLASS_TYPE( ( klass ), HYSCAN_TYPE_CACHED ) )
#define HYSCAN_CACHED_GET_CLASS( obj )          ( G_TYPE_INSTANCE_GET_CLASS( ( obj ), HYSCAN_TYPE_CACHED, HyScanCachedClass ) )


typedef GObject HyScanCached;
typedef GObjectClass HyScanCachedClass;


GType hyscan_cached_get_type( void );

/*!
 *
 * Функция создаёт новый объект \link HyScanCached \endlink.
 *
 * \param cache_size максимальный объём памяти, Мб.
 *
 * \return Указатель на объект \link HyScanCached \endlink.
 *
*/
HyScanCached *hyscan_cached_new( guint32 cache_size );


G_END_DECLS

#endif // _hyscan_cached_h
