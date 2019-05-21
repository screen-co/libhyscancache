/* hyscan-cache.c
 *
 * Copyright 2015-2019 Screen LLC, Andrei Fadeev <andrei@webcontrol.ru>
 *
 * This file is part of HyScanCache.
 *
 * HyScanCache is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * HyScanCache is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Alternatively, you can license this code under a commercial license.
 * Contact the Screen LLC in this case - <info@screen-co.ru>.
 */

/* HyScanCache имеет двойную лицензию.
 *
 * Во-первых, вы можете распространять HyScanCache на условиях Стандартной
 * Общественной Лицензии GNU версии 3, либо по любой более поздней версии
 * лицензии (по вашему выбору). Полные положения лицензии GNU приведены в
 * <http://www.gnu.org/licenses/>.
 *
 * Во-вторых, этот программный код можно использовать по коммерческой
 * лицензии. Для этого свяжитесь с ООО Экран - <info@screen-co.ru>.
 */

/**
 * SECTION: hyscan-cache
 * @Short_description: интерфейс системы кэширования
 * @Title: HyScanCache
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
 */

#include "hyscan-cache.h"
#include "hyscan-hash.h"

G_DEFINE_INTERFACE (HyScanCache, hyscan_cache, G_TYPE_OBJECT);

static void
hyscan_cache_default_init (HyScanCacheInterface *iface)
{
}

/**
 * hyscan_cache_set:
 * @cache: указатель на #HyScanCache
 * @key ключ объекта
 * @detail: (nullable): вспомогательная информация
 * @buffer: (nullable): указатель на сохраняемые данные
 *
 * Функция помещает данные в кэш. Если переменная detail = NULL, вспомогательная
 * информация не будет учитываться для этого объекта. Если buffer = NULL, объект
 * удаляется из кэша.
 *
 * Returns: %TRUE если сохранены в кэше, иначе %FALSE.
 */
gboolean
hyscan_cache_set  (HyScanCache * cache,
                   const gchar  *key,
                   const gchar  *detail,
                   HyScanBuffer *buffer)
{
  if (HYSCAN_CACHE_GET_IFACE (cache)->set != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->set (cache,
                                                  hyscan_hash64 (key),
                                                  hyscan_hash64 (detail),
                                                  buffer, NULL);
    }

  return FALSE;
}

/**
 * hyscan_cache_set2:
 * @cache: указатель на #HyScanCache
 * @key ключ объекта
 * @detail: (nullable): вспомогательная информация
 * @buffer1: (nullable): указатель на первую часть сохраняемых данных
 * @buffer2: (nullable): указатель на вторую часть сохраняемых данных
 *
 * Функция помещает данные из двух разных мест в одну запись кэша. Если
 * переменная detail = NULL, вспомогательная информация не будет учитываться
 * для этого объекта. Если buffer1 = NULL, объект удаляется из кэша.
 *
 * Returns: %TRUE если сохранены в кэше, иначе %FALSE.
 */
gboolean
hyscan_cache_set2 (HyScanCache  *cache,
                   const gchar  *key,
                   const gchar  *detail,
                   HyScanBuffer *buffer1,
                   HyScanBuffer *buffer2)
{
  if (HYSCAN_CACHE_GET_IFACE (cache)->set != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->set (cache,
                                                  hyscan_hash64 (key),
                                                  hyscan_hash64 (detail),
                                                  buffer1, buffer2);
    }

  return FALSE;
}

/**
 * hyscan_cache_set2i:
 * @cache: указатель на #HyScanCache
 * @key ключ объекта
 * @detail: (nullable): вспомогательная информация
 * @buffer1: (nullable): указатель на первую часть сохраняемых данных
 * @buffer2: (nullable): указатель на вторую часть сохраняемых данных
 *
 * Функция помещает данные из двух разных мест в одну запись кэша. Функция
 * работает аналогично функции #hyscan_cache_set2.
 *
 * Returns: %TRUE если сохранены в кэше, иначе %FALSE.
 */
gboolean
hyscan_cache_set2i (HyScanCache  *cache,
                    guint64       key,
                    guint64       detail,
                    HyScanBuffer *buffer1,
                    HyScanBuffer *buffer2)
{
  if (HYSCAN_CACHE_GET_IFACE (cache)->set != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->set (cache,
                                                  key, detail,
                                                  buffer1, buffer2);
    }

  return FALSE;
}

/**
 * hyscan_cache_get:
 * @cache: указатель на #HyScanCache
 * @key ключ объекта
 * @detail: (nullable): вспомогательная информация
 * @buffer: указатель на буфер для записи данных
 *
 * Функция считывает данные из кэша. Если переменная detail = NULL,
 * вспомогательная информация не будет учитываться для этого объекта.
 *
 * Returns: %TRUE если данные считаны из кэша, иначе %FALSE.
 */
gboolean
hyscan_cache_get  (HyScanCache  *cache,
                   const gchar  *key,
                   const gchar  *detail,
                   HyScanBuffer *buffer)
{
  if( HYSCAN_CACHE_GET_IFACE (cache)->get != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->get (cache,
                                                  hyscan_hash64 (key),
                                                  hyscan_hash64 (detail),
                                                  G_MAXUINT32, buffer, NULL);
    }

  return FALSE;
}

/**
 * hyscan_cache_get2:
 * @cache: указатель на #HyScanCache
 * @key ключ объекта
 * @detail: (nullable): вспомогательная информация
 * @size1 размер данных в первом буфере;
 * @buffer1: указатель на первый буфер для записи данных
 * @buffer2: указатель на второй буфер для записи данных
 *
 * Функция считывает данные из кэша в два разных буфера. В первый буфер будут
 * записаны данные размером не более size1, оставшиеся данные будут записаны
 * во второй буфер. Если переменная detail = NULL, вспомогательная информация
 * не будет учитываться для этого объекта.
 *
 * Returns: %TRUE если данные считаны из кэша, иначе %FALSE.
 */
gboolean
hyscan_cache_get2 (HyScanCache  *cache,
                   const gchar  *key,
                   const gchar  *detail,
                   guint32       size1,
                   HyScanBuffer *buffer1,
                   HyScanBuffer *buffer2)
{
  if( HYSCAN_CACHE_GET_IFACE (cache)->get != NULL)
    {
      return HYSCAN_CACHE_GET_IFACE (cache)->get (cache,
                                                  hyscan_hash64 (key),
                                                  hyscan_hash64 (detail),
                                                  size1, buffer1, buffer2);
    }

  return FALSE;
}

/**
 * hyscan_cache_get2:
 * @cache: указатель на #HyScanCache
 * @key ключ объекта
 * @detail: (nullable): вспомогательная информация
 * @size1 размер данных в первом буфере;
 * @buffer1: указатель на первый буфер для записи данных
 * @buffer2: указатель на второй буфер для записи данных
 *
 * Функция считывает данные из кэша в два разных буфера. Функция работает
 * аналогично функции #hyscan_cache_get2.
 *
 * Returns: %TRUE если данные считаны из кэша, иначе %FALSE.
 */
gboolean
hyscan_cache_get2i (HyScanCache  *cache,
                    guint64       key,
                    guint64       detail,
                    guint32       size1,
                    HyScanBuffer *buffer1,
                    HyScanBuffer *buffer2)
{
  if( HYSCAN_CACHE_GET_IFACE (cache)->get != NULL)
    {
     return HYSCAN_CACHE_GET_IFACE (cache)->get (cache,
                                                 key, detail,
                                                 size1, buffer1, buffer2);
    }

  return FALSE;
}
