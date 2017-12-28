/**
 * \file hyscan-cache.h
 *
 * \brief Заголовочный файл интерфейса системы кэширования HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
 * \defgroup HyScanCache HyScanCache - интерфейс системы кэширования
 *
 * Интерфейс HyScanCache предназначен для взаимодействия с системой кэширования
 * данных. Кэширование данных, полученных в результате длительной обработки,
 * может значительно увеличить производительность программы и исключить повторное
 * выполнение работы.
 *
 * Система кэширования идентифицирует сохраняемые данные по уникальному ключу.
 * Для каждого сохраняемого объекта (данных) может быть указана вспомогательная
 * информация, по которой можно определить параметры обработки данных и принять
 * решение о возможности их использования.
 *
 * Ключ объекта и вспомогательная информация указываются в виде строк, но в
 * системе кэширования храняться в виде 64-х битного хэша (FarmHash). Например,
 * при хранении в кэше строк акустичеких данных, обработанных с учетом коррекции
 * яркости, можно использовать следующие ключ и вспомогательную информацию:
 *
 * - ключ - "Project.Track.Channel.Line.Index";
 * - вспомогательная информация - "Brightness=1.25,Contrast=0.8",
 *
 * где Project, Track, Channel - названия проекта, галса и канала данных,
 * Index - номер строки зондирования, числа после Brightness и Contrast -
 * параметры алгоритма коррекции яркости.
 *
 * Размещение данных в кэше осуществляется функцией #hyscan_cache_set, чтение
 * данных функцией #hyscan_cache_get.
 *
 * Имеется возможность разместить данные в кэше из двух разных мест, например,
 * данные и их параметры. Для этого используется функция #hyscan_cache_set2.
 * Считать такие данные в два разных буфера можно функцией #hyscan_cache_get2.
 * Пользователь должен знать структуру данных в кэше, в частности размеры каждой
 * из частей.
 *
 * Существуют функции #hyscan_cache_set2i и #hyscan_cache_get2i, аналогичные
 * функциям #hyscan_cache_set2 и #hyscan_cache_get2, но отличающиеся способом
 * задания ключа и вспомогательной информации. В этих функциях ключ и
 * вспомогательная информация задаются как 64-х битное целое беззнаковое число.
 *
 */

#ifndef __HYSCAN_CACHE_H__
#define __HYSCAN_CACHE_H__

#include <glib-object.h>
#include <hyscan-buffer.h>

G_BEGIN_DECLS

#define HYSCAN_TYPE_CACHE            (hyscan_cache_get_type ())
#define HYSCAN_CACHE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (( obj ), HYSCAN_TYPE_CACHE, HyScanCache))
#define HYSCAN_IS_CACHE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (( obj ), HYSCAN_TYPE_CACHE))
#define HYSCAN_CACHE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE (( obj ), HYSCAN_TYPE_CACHE, HyScanCacheInterface))

typedef struct _HyScanCache HyScanCache;

typedef struct _HyScanCacheInterface HyScanCacheInterface;

struct _HyScanCacheInterface
{
  GTypeInterface g_iface;

  gboolean     (*set)                  (HyScanCache           *cache,
                                        guint64                key,
                                        guint64                detail,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

  gboolean     (*get)                  (HyScanCache           *cache,
                                        guint64                key,
                                        guint64                detail,
                                        guint32                size1,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);
};

HYSCAN_API
GType          hyscan_cache_get_type   (void);

/**
 *
 * Функция помещает данные в кэш. Если переменная detail = NULL, вспомогательная
 * информация не будет учитываться для этого объекта. Если buffer = NULL, объект
 * удаляется из кэша.
 *
 * \param cache указатель на интерфейс \link HyScanCache \endlink;
 * \param key ключ объекта;
 * \param detail вспомогательная информация или NULL;
 * \param buffer указатель на сохраняемые данные или NULL.
 *
 * \return TRUE - если данные успешно сохранены в кэше, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean       hyscan_cache_set        (HyScanCache           *cache,
                                        const gchar           *key,
                                        const gchar           *detail,
                                        HyScanBuffer          *buffer);

/**
 *
 * Функция помещает данные из двух разных мест в одну запись кэша. Если
 * переменная detail = NULL, вспомогательная информация не будет учитываться
 * для этого объекта. Если buffer1 = NULL, объект удаляется из кэша.
 *
 * \param cache указатель на интерфейс \link HyScanCache \endlink;
 * \param key ключ объекта;
 * \param detail вспомогательная информация или NULL;
 * \param buffer1 указатель на первую часть сохраняемых данных или NULL;
 * \param buffer2 указатель на вторую часть сохраняемых данных или NULL.
 *
 * \return TRUE - если данные успешно сохранены в кэше, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean       hyscan_cache_set2       (HyScanCache           *cache,
                                        const gchar           *key,
                                        const gchar           *detail,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

/**
 *
 * Функция помещает данные из двух разных мест в одну запись кэша. Если
 * переменная detail = 0, вспомогательная информация не будет учитываться
 * для этого объекта. Если buffer1 = NULL, объект удаляется из кэша.
 *
 * \param cache указатель на интерфейс \link HyScanCache \endlink;
 * \param key ключ объекта;
 * \param detail вспомогательная информация или NULL;
 * \param buffer1 указатель на первую часть сохраняемых данных или NULL;
 * \param buffer2 указатель на вторую часть сохраняемых данных или NULL.
 *
 * \return TRUE - если данные успешно сохранены в кэше, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean       hyscan_cache_set2i      (HyScanCache           *cache,
                                        guint64                key,
                                        guint64                detail,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

/**
 *
 * Функция считывает данные из кэша. Если переменная detail = NULL,
 * вспомогательная информация не будет учитываться для этого объекта.
 *
 * \param cache указатель на интерфейс \link HyScanCache \endlink;
 * \param key ключ объекта;
 * \param detail вспомогательная информация или NULL;
 * \param buffer указатель на буфер для записи данных.
 *
 * \return TRUE - если данные найдены в кэше и считаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean       hyscan_cache_get        (HyScanCache           *cache,
                                        const gchar           *key,
                                        const gchar           *detail,
                                        HyScanBuffer          *buffer);

/**
 *
 * Функция считывает данные из кэша в два разных буфера. В первый буфер будут
 * записаны данные размером не более size1, оставшиеся данные будут записаны
 * во второй буфер. Если переменная detail = NULL, вспомогательная информация
 * не будет учитываться для этого объекта.
 *
 * \param cache указатель на интерфейс \link HyScanCache \endlink;
 * \param key ключ объекта;
 * \param detail вспомогательная информация или NULL;
 * \param size1 размер данных в первом буфере;
 * \param buffer1 указатель на первый буфер для записи данных;
 * \param buffer2 указатель на второй буфер для записи данных.
 *
 * \return TRUE - если данные найдены в кэше и считаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean       hyscan_cache_get2       (HyScanCache           *cache,
                                        const gchar           *key,
                                        const gchar           *detail,
                                        guint32                size1,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

/**
 *
 * Функция считывает данные из кэша в два разных буфера. Функция работает
 * аналогично функции #hyscan_cache_get2.
 *
 * \param cache указатель на интерфейс \link HyScanCache \endlink;
 * \param key ключ объекта;
 * \param detail вспомогательная информация или NULL;
 * \param size1 размер данных в первом буфере;
 * \param buffer1 указатель на первый буфер для записи данных;
 * \param buffer2 указатель на второй буфер для записи данных или NULL.
 *
 * \return TRUE - если данные найдены в кэше и считаны, FALSE - в случае ошибки.
 *
 */
HYSCAN_API
gboolean       hyscan_cache_get2i      (HyScanCache           *cache,
                                        guint64                key,
                                        guint64                detail,
                                        guint32                size1,
                                        HyScanBuffer          *buffer1,
                                        HyScanBuffer          *buffer2);

G_END_DECLS

#endif /* __HYSCAN_CACHE_H__ */
