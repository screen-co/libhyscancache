
#include "hyscan-hash.h"
#include "farmhash.h"
#include <string.h>

guint64
hyscan_hash64 (const gchar *name)
{

  return name != NULL ? util::Hash64 (name, strlen (name)) : 0;

}
