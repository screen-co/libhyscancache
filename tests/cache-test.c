#include <hyscan-cache-server.h>
#include <hyscan-cache-client.h>
#include <hyscan-cached.h>
#include <string.h>

#define MAX_THREADS (32)
#define MAX_SIZE    (1024 * 1024)
#define MIN_SIZE    (4)

gdouble duration = 10.0;
gint cache_size = 0;
gint n_patterns = 0;
gint n_threads = 0;
gint n_objects = 0;
gint small_size = 0;
gint big_size = 0;
gboolean rpc = FALSE;
gboolean preload = FALSE;
gboolean update = FALSE;

HyScanCache *cache[MAX_THREADS+2];
HyScanCacheServer *server = NULL;

gint pattern_size;
guint8 **patterns;

gint started_threads = 0;
gint start = 0;
gint stop = 0;

/* Запись данных в кэш. */
gpointer
data_writer (gpointer thread_data)
{
  HyScanBuffer *buffer1;
  HyScanBuffer *buffer2;
  gint data_index;
  gchar key[16];
  gint i;

  buffer1 = hyscan_buffer_new ();
  buffer2 = hyscan_buffer_new ();

  /* Сигнализация запуска потока. */
  g_atomic_int_inc (&started_threads);
  data_index = GPOINTER_TO_INT (thread_data);
  g_message ("starting %s data writer thread", data_index ? "big" : "small");

  /* Загрузка кэша до начала тестирования. */
  if (preload)
    {
      g_message ("preloading %s data", data_index ? "big" : "small");
      for (i = data_index; i < n_objects; i += 2)
        {
          gpointer data = patterns[i % n_patterns];
          gint32 size1 = (data_index ? big_size : small_size);
          gint32 size2 = size1 * g_random_double_range (0.5, 1.0);

          hyscan_buffer_wrap (buffer1, HYSCAN_DATA_BLOB, data, size1);
          hyscan_buffer_wrap (buffer2, HYSCAN_DATA_BLOB, data, size2);

          g_snprintf (key, sizeof(key), "%09d", i);
          if (!hyscan_cache_set2 (cache[data_index], key, NULL, buffer1, buffer2))
            g_message ("data_writer: '%s' set error", key);
        }
    }

  /* Сигнализация о завершении загрузки кэша. */
  g_atomic_int_inc (&start);
  while (g_atomic_int_get (&start) != 3)
    g_usleep (1000);

  /* Обновление кэша. */
  while (update && g_atomic_int_get (&stop) == 0)
    {
      gint key_id = 2 * g_random_int_range (0, n_objects / 2) + data_index;
      gpointer data = patterns[key_id % n_patterns];
      gint32 size1 = (data_index ? big_size : small_size);
      gint32 size2 = size1 * g_random_double_range (0.5, 1.0);

      hyscan_buffer_wrap (buffer1, HYSCAN_DATA_BLOB, data, size1);
      hyscan_buffer_wrap (buffer2, HYSCAN_DATA_BLOB, data, size2);

      g_snprintf (key, sizeof (key), "%09d", key_id);
      if (!hyscan_cache_set2 (cache[data_index], key, NULL, buffer1, buffer2))
        g_message ("data_writer: '%s' set error", key);

      g_usleep (1);
    }

  g_object_unref (buffer1);
  g_object_unref (buffer2);

  return NULL;
}

/* Чтение данных из кэша. */
gpointer
data_reader (gpointer data)
{
  static volatile gint running_threads = 0;

  HyScanBuffer *buffer1;
  HyScanBuffer *buffer2;

  gint thread_id;

  GTimer *timer = g_timer_new ();
  gdouble hit_time = 0.0;
  gdouble miss_time = 0.0;
  guint hit = 0;
  guint miss = 0;;

  /* Идентификатор потока. */
  thread_id = g_atomic_int_add (&running_threads, 1);

  /* Буферы данных. */
  buffer1 = hyscan_buffer_new ();
  buffer2 = hyscan_buffer_new ();

  /* Сигнализация запуска потока. */
  g_atomic_int_inc (&started_threads);
  g_message ("starting reader thread %d", thread_id);

  /* Ожидаем запуска всех потоков и завершения загрузки кэша. */
  while (g_atomic_int_get (&start) != 3)
    g_usleep (1000);

  /* Работа с кэшем. */
  while (g_atomic_int_get (&stop) == 0)
    {
      gpointer data1, data2;
      guint32 size1, size2;
      gboolean status;
      gdouble req_time;
      gchar key[16];
      gint key_id;

      key_id = g_random_int_range (0, n_objects);
      g_snprintf (key, sizeof (key), "%09d", key_id);

      g_timer_start (timer);
      size1 = ((key_id % 2) ? big_size : small_size);
      status = hyscan_cache_get2 (cache[thread_id+2], key, NULL, size1, buffer1, buffer2);
      req_time = g_timer_elapsed (timer, NULL);

      if (status)
        {
          data1 = hyscan_buffer_get (buffer1, NULL, &size1);
          data2 = hyscan_buffer_get (buffer2, NULL, &size2);

          /* Проверка размера данных. */
          if ((size1 < size2) || (size1 != ((guint)((key_id % 2) ? big_size : small_size))))
            {
              g_error ("test thread %d: '%s' size mismatch %d, %d != %d",
                       thread_id, key, size1, size2, key_id % 2 ? big_size : small_size);
            }
          /* Проверка данных. */
          else if (memcmp (data1, patterns[key_id % n_patterns], size1))
            {
              g_error ("test thread %d: '%s' data1 mismatch", thread_id, key);
            }
          else if (memcmp (data2, patterns[key_id % n_patterns], size2))
            {
              g_error ("test thread %d: '%s' data2 mismatch", thread_id, key);
            }
          else
            {
              hit_time += req_time;
              hit += 1;
            }
        }
      else
        {
          miss_time += req_time;
          miss += 1;
        }

    }

  g_timer_destroy (timer);

  g_object_unref (buffer1);
  g_object_unref (buffer2);

  g_message ("thread %d: hits: number = %d time = %.3lf us/req, misses: number = %d time = %.3lf us/req",
             thread_id, hit, (1000000.0 * hit_time) / hit, miss, (1000000.0 * miss_time) / miss);

  return NULL;
}

int
main (int argc, char **argv)
{
  HyScanCached *cached;
  GThread *small_data_writer_thread;
  GThread *big_data_writer_thread;
  GThread **threads;
  GTimer *timer;

  gint i, j;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "duration", 'd', 0, G_OPTION_ARG_DOUBLE, &duration, "Test duration, seconds", NULL },
        { "cache-size", 'm', 0, G_OPTION_ARG_INT, &cache_size, "Cache size, Mb", NULL },
        { "rpc", 'c', 0, G_OPTION_ARG_NONE, &rpc, "Use rpc interface", NULL },
        { "preload", 'l', 0, G_OPTION_ARG_NONE, &preload, "Preload cache with data", NULL },
        { "patterns", 'p', 0, G_OPTION_ARG_INT, &n_patterns, "Number of testing patterns", NULL },
        { "threads", 't', 0, G_OPTION_ARG_INT, &n_threads, "Number of working threads", NULL },
        { "updates", 'u', 0, G_OPTION_ARG_NONE, &update, "Update cache data during test", NULL },
        { "objects", 'o', 0, G_OPTION_ARG_INT, &n_objects, "Number of unique objects", NULL },
        { "small-size", 's', 0, G_OPTION_ARG_INT, &small_size, "Maximum small objects size, bytes", NULL },
        { "big-size", 'b', 0, G_OPTION_ARG_INT, &big_size, "Maximum big objects size, bytes", NULL },
        { NULL } };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_print ("%s\n", error->message);
        return -1;
      }

    if ((duration < 1.0) || (cache_size == 0) ||
        (n_patterns == 0) || (n_threads == 0) || (n_objects == 0) ||
        (small_size == 0) || (big_size == 0))
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    g_strfreev (args);
  }

  /* Ограничения. */
  if (n_threads > MAX_THREADS)
    n_threads = MAX_THREADS;
  if (small_size > MAX_SIZE)
    small_size = MAX_SIZE;
  if (small_size < MIN_SIZE)
    small_size = MIN_SIZE;
  if (small_size % 2 != 0)
    small_size -= 1;

  if (big_size > MAX_SIZE)
    big_size = MAX_SIZE;
  if (big_size < MIN_SIZE)
    big_size = MIN_SIZE;

  if (big_size % 2 != 0)
    big_size -= 1;

  /* Создаём кэш. */
  cached = hyscan_cached_new (cache_size);
  if (rpc)
    {
      server = hyscan_cache_server_new ("shm://local", HYSCAN_CACHE (cached),
                                        n_threads, n_threads + 2);
      if (!hyscan_cache_server_start(server))
        g_error ("can't start cache server");

      for (i = 0; i < n_threads + 2; i++)
        cache[i] = HYSCAN_CACHE (hyscan_cache_client_new ("shm://local"));
    }
  else
    {
      for (i = 0; i < n_threads + 2; i++)
        cache[i] = HYSCAN_CACHE (g_object_ref (cached));
    }

  /* Шаблоны тестирования. */
  g_message ("creating test patterns");
  pattern_size = big_size > small_size ? big_size : small_size;
  patterns = g_malloc (n_patterns * sizeof(gint8*));
  for (i = 0; i < n_patterns; i++)
    {
      patterns[i] = g_malloc (pattern_size);
      for (j = 0; j < pattern_size; j++)
        patterns[i][j] = g_random_int ();
    }

  /* Потоки записи данных в кэш. */
  small_data_writer_thread = g_thread_new ("test-thread", data_writer, GINT_TO_POINTER( 0 ));
  big_data_writer_thread = g_thread_new ("test-thread", data_writer, GINT_TO_POINTER( 1 ));

  /* Потоки тестирования. */
  threads = g_malloc (n_threads * sizeof(GThread*));
  for (i = 0; i < n_threads; i++)
    threads[i] = g_thread_new ("test-thread", data_reader, NULL);

  /* Ожидаем запуска всех потоков. */
  while (g_atomic_int_get (&started_threads) != n_threads + 2)
    g_usleep (1000);

  /* Ожидаем завершение загрузки данных в кэш. */
  while (g_atomic_int_get (&start) != 2)
    g_usleep (1000);

  /* Запуск тестирования. */
  g_message ("begin testing");
  g_atomic_int_inc (&start);

  /* Тестирование в течение указанного времени. */
  timer = g_timer_new ();
  while (g_timer_elapsed (timer, NULL) < duration)
    g_usleep (10000);

  /* Сигнализация о завершении теста. */
  g_atomic_int_set (&stop, 1);

  g_timer_destroy (timer);

  /* Ожидаем завершения работы потоков. */
  for (i = 0; i < n_threads; i++)
    g_thread_join (threads[i]);
  g_free (threads);

  /* Завершаем потоки обновления данных. */
  g_atomic_int_add (&start, -1);

  g_thread_join (small_data_writer_thread);
  g_thread_join (big_data_writer_thread);

  for (i = 0; i < n_patterns; i++)
    g_free (patterns[i]);
  g_free (patterns);

  for (i = 0; i < n_threads + 2; i++)
    g_clear_object (&cache[i]);
  g_clear_object (&server);
  g_clear_object (&cached);

  return 0;
}
