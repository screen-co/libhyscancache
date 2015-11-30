#include <hyscan-cached.h>
#include <string.h>

gint cache_size = 0;
gint n_patterns = 0;
gint n_threads = 0;
gint n_requests = 0;
gint n_objects = 0;
gint small_size = 0;
gint big_size = 0;
gboolean update = FALSE;

HyScanCache *cache;

gint pattern_size;
guint8 **patterns;
guint8 **buffers;

volatile gint start = 0;

/* Запись данных в кэш. */
gpointer
data_writer (gpointer data)
{
  gint data_index = GPOINTER_TO_INT( data );
  gchar key[16];
  gint i;

  /* Загрузка кэша до начала тестирования. */
  for (i = data_index; i < n_objects; i += 2)
    {
      gpointer data = patterns[i % n_patterns];
      gint32 size = data_index ? big_size : small_size;

      g_snprintf (key, sizeof(key), "%09d", i);
      if (!hyscan_cache_set2 (cache, key, NULL, data, size, data, size))
        g_message ("data_writer: '%s' set error", key);
    }

  /* Сигнализация о завершении загрузки кэша. */
  g_atomic_int_inc (&start);
  while (g_atomic_int_get (&start) != 3);

  /* Обновление кэша. */
  while (update && g_atomic_int_get (&start) == 3)
    {
      gint key_id = 2 * g_random_int_range (0, n_objects / 2) + data_index;
      gpointer data = patterns[key_id % n_patterns];
      gint32 size = data_index ? big_size : small_size;

      g_snprintf (key, sizeof (key), "%09d", key_id);
      if (!hyscan_cache_set2 (cache, key, NULL, data, size, data, size))
        g_message ("data_writer: '%s' set error", key);

      g_usleep (1);
    }

  return NULL;
}

/* Чтение данных из кэша. */
gpointer
data_reader (gpointer data)
{
  static volatile gint running_threads = 0;

  gint thread_id;

  GTimer *timer = g_timer_new ();
  gdouble hit_time, miss_time;
  gint hit, miss;

  gint i;

  /* Идентификатор потока. */
  thread_id = g_atomic_int_add (&running_threads, 1);

  /* Ожидаем запуска всех потоков и завершения загрузки кэша. */
  while (g_atomic_int_get (&start) != 3);
  g_message ("starting thread %d", thread_id);

  hit_time = 0.0;
  miss_time = 0.0;
  hit = 0;
  miss = 0;

  // Работа с кэшем.
  for (i = 0; i < n_requests; i++)
    {
      gint32 size1, size2;
      gboolean status;
      gdouble req_time;
      gchar key[16];
      gint key_id;

      key_id = g_random_int_range (0, n_objects);
      g_snprintf (key, sizeof (key), "%09d", key_id);

      g_timer_start (timer);
      size1 = size2 = ((key_id % 2) ? big_size : small_size);
      status = hyscan_cache_get2 (cache, key, NULL,
                                  buffers[thread_id], &size1,
                                  buffers[thread_id] + size1, &size2);
      req_time = g_timer_elapsed (timer, NULL);

      if (status)
        {
          // Проверка размера данных.
          if (size1 != size2 || size1 != ((key_id % 2) ? big_size : small_size))
            {
              g_message ("test thread %d: '%s' size mismatch %d != %d",
                         thread_id, key, size1, key_id % 2 ? big_size : small_size);
            }
          // Проверка данных.
          else if (memcmp (buffers[thread_id], patterns[key_id % n_patterns], size1))
            {
              g_message ("test thread %d: '%s' data1 mismatch", thread_id, key);
            }
          else if (memcmp (buffers[thread_id] + size1, patterns[key_id % n_patterns], size2))
            {
              g_message ("test thread %d: '%s' data2 mismatch", thread_id, key);
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

  g_message ("thread %d: hits: number = %d time = %.3lf ms/req, misses: number = %d time = %.3lf ms/req",
             thread_id, hit, (1000.0 * hit_time) / hit, miss, (1000.0 * miss_time) / miss);

  return NULL;
}

int
main (int argc, char **argv)
{
  GThread *small_data_writer_thread;
  GThread *big_data_writer_thread;
  GThread **threads;

  gint i, j;

  /* Разбор командной строки. */
  {
    gchar **args;
    GError *error = NULL;
    GOptionContext *context;
    GOptionEntry entries[] =
      {
        { "cache-size", 'm', 0, G_OPTION_ARG_INT, &cache_size, "Cache size, Mb", NULL },
        { "patterns", 'p', 0, G_OPTION_ARG_INT, &n_patterns, "Number of testing patterns", NULL },
        { "threads", 't', 0, G_OPTION_ARG_INT, &n_threads, "Number of working threads", NULL },
        { "updates", 'u', 0, G_OPTION_ARG_NONE, &update, "Update cache data during test", NULL },
        { "requests", 'r', 0, G_OPTION_ARG_INT, &n_requests, "Number of requests", NULL },
        { "objects", 'o', 0, G_OPTION_ARG_INT, &n_objects, "Number of unique objects", NULL },
        { "small-size", 's', 0, G_OPTION_ARG_INT, &small_size, "Maximum small objects size, bytes", NULL },
        { "big-size", 'b', 0, G_OPTION_ARG_INT, &big_size, "Maximum big objects size, bytes", NULL },
        { NULL } };

#ifdef G_OS_WIN32
    args = g_win32_get_command_line ();
#else
    args = g_strdupv (argv);
#endif

    context = g_option_context_new ("[uri]");
    g_option_context_set_help_enabled (context, TRUE);
    g_option_context_add_main_entries (context, entries, NULL);
    g_option_context_set_ignore_unknown_options (context, FALSE);
    if (!g_option_context_parse_strv (context, &args, &error))
      {
        g_message( error->message);
        return -1;
      }

    if (!cache_size || !n_patterns || !n_threads || !n_requests || !n_objects || !small_size || !big_size)
      {
        g_print ("%s", g_option_context_get_help (context, FALSE, NULL));
        return 0;
      }

    g_option_context_free (context);

    g_strfreev (args);
  }

  /* Ограничения. */
  if (n_threads > 32)
    n_threads = 32;
  if (small_size > 1024 * 1024)
    small_size = 1024 * 1024;
  if (big_size > 1024 * 1024)
    big_size = 1024 * 1024;

  /* Создаём кэш. */
  cache = HYSCAN_CACHE( hyscan_cached_new( cache_size ) );

  /* Шаблоны тестирования. */
  pattern_size = big_size > small_size ? big_size : small_size;
  patterns = g_malloc (n_patterns * sizeof(gint8*));
  for (i = 0; i < n_patterns; i++)
    {
      patterns[i] = g_malloc (pattern_size);
      for (j = 0; j < pattern_size; j++)
        patterns[i][j] = g_random_int ();
    }

  /* Буферы обмена. */
  buffers = g_malloc (n_threads * sizeof(gint8*));
  for (i = 0; i < n_threads; i++)
    buffers[i] = g_malloc (2 * pattern_size);

  /* Потоки записи данных в кэш. */
  small_data_writer_thread = g_thread_new ("test-thread", data_writer, GINT_TO_POINTER( 0 ));
  big_data_writer_thread = g_thread_new ("test-thread", data_writer, GINT_TO_POINTER( 1 ));

  // Ожидаем завершение загрузки данных в кэш.
  while (g_atomic_int_get (&start) != 2);

  // Потоки тестирования.
  threads = g_malloc (n_threads * sizeof(GThread*));
  for (i = 0; i < n_threads; i++)
    threads[i] = g_thread_new ("test-thread", data_reader, NULL);

  // Запуск тестирования.
  g_message( "start...");
  g_atomic_int_inc (&start);

  // Ожидаем завершения работы потоков.
  for (i = 0; i < n_threads; i++)
    g_thread_join (threads[i]);
  g_free (threads);

  // Завершаем потоки обновления данных.
  g_atomic_int_add (&start, -1);

  g_thread_join (small_data_writer_thread);
  g_thread_join (big_data_writer_thread);

  for (i = 0; i < n_threads; i++)
    g_free (buffers[i]);
  for (i = 0; i < n_patterns; i++)
    g_free (patterns[i]);
  g_free (buffers);
  g_free (patterns);

  g_object_unref (cache);

  return 0;
}
