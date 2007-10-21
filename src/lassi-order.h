#ifndef foolassiorderhfoo
#define foolassiorderhfoo

#include <glib.h>

int lassi_list_compare(GList *i, GList *j);
gboolean lassi_list_nodups(GList *l);
GList *lassi_list_merge(GList *a, GList *b);
GList *lassi_list_copy(GList *l);
void lassi_list_free(GList *i);

#endif

