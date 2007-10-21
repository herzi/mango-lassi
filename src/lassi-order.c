#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib.h>

#include "lassi-order.h"

int lassi_list_compare(GList *i, GList *j) {

    for (; i && j; i = i->next, j = j->next) {
        int c;

        c = strcmp(i->data, j->data);

        if (c)
            return c;

    }

    if (i)
        return 1;

    if (j)
        return -1;

    return 0;
}

gboolean lassi_list_nodups(GList *l) {
    GList *i, *j;

    for (i = l; i; i = i->next)
        for (j = i->next; j; j = j->next)
            if (strcmp(i->data, j->data) == 0)
                return FALSE;

    return TRUE;
}

GList *lassi_list_merge(GList *a, GList *b) {
    GList *ia, *ib, *p, *c, *d;

    g_assert(lassi_list_nodups(a));
    g_assert(lassi_list_nodups(b));

    p = b;

    for (ia = a; ia; ia = ia->next) {

        for (ib = p; ib; ib = ib->next) {

            if (strcmp(ia->data, ib->data) == 0) {

                /* Found a common entry, hence copy everything since
                 * the last one we found from b to a */

                for (c = p; c != ib; c = c->next) {

                    /* Before we copy, make sure this entry is not yet
                     * in a */

                    for (d = a; d; d = d->next)
                        if (strcmp(c->data, d->data) == 0)
                            break;

                    if (!d)
                        /* OK, This one is new, let's copy it */

                        a = g_list_insert_before(a, ia, g_strdup(c->data));
                }

                p = ib->next;
            }
        }
    }

    /* Copy the tail */
    for (c = p; c; c = c->next) {

        for (d = a; d; d = d->next)
            if (strcmp(c->data, d->data) == 0)
                break;

        if (!d)
            a = g_list_append(a, g_strdup(c->data));
    }

    g_assert(lassi_list_nodups(a));

    return a;
}

GList* lassi_list_copy(GList *l) {
    GList *r = NULL;

    for (; l; l = l->next)
        r = g_list_prepend(r, g_strdup(l->data));

    return g_list_reverse(r);
}

void lassi_list_free(GList *i) {
    while (i) {
        g_free(i->data);
        i = i->next;
    }
}


#if 0

int main(int argc, char *argv[]) {
    GList *a = NULL, *b = NULL, *c = NULL, *d = NULL, *i;


    a = g_list_append(a, "eins");
    a = g_list_append(a, "zwei");
    a = g_list_append(a, "vier");
    a = g_list_append(a, "fünf");
    a = g_list_append(a, "sechs");
    a = g_list_append(a, "acht");

    b = g_list_append(b, "eins");
    b = g_list_append(b, "zwei");
    b = g_list_append(b, "drei");
    b = g_list_append(b, "vier");
    b = g_list_append(b, "sechs");
    b = g_list_append(b, "acht");

    c = g_list_append(c, "eins");
    c = g_list_append(c, "sieben");
    c = g_list_append(c, "acht");

    d = g_list_append(d, "drei");
    d = g_list_append(d, "neun");
    d = g_list_append(d, "zwei");

    a = lassi_list_merge(a, b);
    a = lassi_list_merge(a, c);
    a = lassi_list_merge(a, d);

    for (i = a; i; i = i->next)
        g_debug("%s", (char*) i->data);

    return 0;
}

#endif
