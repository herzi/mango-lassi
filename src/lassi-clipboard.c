#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>

#include "lassi-server.h"

#define LASSI_MARKER "application/x-mango-lassi-marker"

#if !GTK_CHECK_VERSION(2,14,0)
#define gtk_selection_data_get_data(sd)   ((sd)->data)
#define gtk_selection_data_get_format(sd) ((sd)->format)
#define gtk_selection_data_get_length(sd) ((sd)->length)
#define gtk_selection_data_get_target(sd) ((sd)->target)
#endif

static void targets_received(GtkClipboard *clipboard, GdkAtom *atoms, int n_atoms, gpointer userdata) {
    int j, k;
    LassiClipboardInfo *i = userdata;
    char **targets;

    g_assert(clipboard);
    g_assert(i);

    g_debug("recvd targs %p, %i", (void*) atoms, n_atoms);

    if (!atoms)
        return;

    targets = g_new0(char*, n_atoms+1);

    for (j = 0, k = 0; j < n_atoms; j++) {
        char *c = gdk_atom_name(atoms[j]);

        /* Avoid loops */
        if (strcmp(c, LASSI_MARKER) == 0) {
            g_free(c);
            goto fail;
        }

        if (strcmp(c, "TIMESTAMP") == 0 ||
            strcmp(c, "TARGETS") == 0 ||
            strcmp(c, "CLIPBOARD_MANAGER") == 0 ||
            strcmp(c, "CLIENT_WINDOW") == 0 ||
            strcmp(c, "DELETE") == 0 ||
            strcmp(c, "INSERT_PROPERTY") == 0 ||
            strcmp(c, "INSERT_SELECTION") == 0 ||
            strcmp(c, "LENGTH") == 0 ||
            strcmp(c, "TASK") == 0 ||
            strcmp(c, "MULTIPLE") == 0 ||
            strcmp(c, "DRAWABLE") == 0) {
            g_free(c);
            continue;
        }

        targets[k++] = c;
    }

    g_debug("%p %i", (void*) targets, n_atoms);
    lassi_server_acquire_clipboard(i->server, clipboard == i->primary, targets);

fail:
    g_strfreev(targets);
}

static void owner_change(GtkClipboard *clipboard, GdkEventOwnerChange *event, gpointer userdata) {
    LassiClipboardInfo *i = userdata;

    g_assert(clipboard);
    g_assert(i);

    g_debug("owner change");

    if (event->reason == GDK_OWNER_CHANGE_NEW_OWNER)
        gtk_clipboard_request_targets(clipboard, targets_received, i);
    else
        lassi_server_return_clipboard(i->server, clipboard == i->primary);
}

static void get_func(GtkClipboard *clipboard, GtkSelectionData *sd, guint info, gpointer userdata) {
    LassiClipboardInfo *i = userdata;
    char *t;
    int f = 0;
    gpointer d = NULL;
    gint l = 0;

    g_assert(clipboard);
    g_assert(i);

    t = gdk_atom_name(gtk_selection_data_get_target(sd));

    g_debug("get(%s)", t);

    if (lassi_server_get_clipboard(i->server, clipboard == i->primary, t, &f, &d, &l) >= 0) {
        g_debug("successfully got data");
        gtk_selection_data_set(sd, gtk_selection_data_get_target(sd), f, d, l);
    } else
        g_debug("failed to get data");

    g_free(d);
    g_free(t);
}

static void clear_func(GtkClipboard *clipboard, gpointer userdata) {
    LassiClipboardInfo *i = userdata;

    g_assert(clipboard);
    g_assert(i);

    g_debug("clear");

    gtk_clipboard_request_targets(clipboard, targets_received, i);
}

int lassi_clipboard_init(LassiClipboardInfo *i, LassiServer *s) {
    GdkDisplay *display;
    GdkScreen  *screen;

    g_assert(i);
    g_assert(s);

    memset(i, 0, sizeof(*i));
    i->server = s;

#ifndef HAS_APP_INDICATOR
    screen = gtk_status_icon_get_screen(s->tray_info.status_icon);
#else
    screen = gdk_screen_get_default();
#endif
    display = gdk_screen_get_display(screen);
    i->clipboard = gtk_clipboard_get_for_display(display, GDK_SELECTION_CLIPBOARD);
    i->primary = gtk_clipboard_get_for_display(display, GDK_SELECTION_PRIMARY);

    g_signal_connect(i->clipboard, "owner_change", G_CALLBACK(owner_change), i);
    g_signal_connect(i->primary, "owner_change", G_CALLBACK(owner_change), i);
    return 0;
}

void lassi_clipboard_done(LassiClipboardInfo *i) {
    g_assert(i);

    memset(i, 0, sizeof(*i));
}

void lassi_clipboard_set(LassiClipboardInfo *i, gboolean primary, char *targets[]) {
    int n = 0, j;
    gboolean b;
    char **t;
    GtkTargetEntry *e;

    for (t = targets; *t; t++)
        n++;

    e = g_new0(GtkTargetEntry, n+1);

    for (t = targets, j = 0; *t; t++, j++) {
        e[j].target = *t;
        e[j].info = j;
    }

    e[j].target = (char*) LASSI_MARKER;
    e[j].info = j;

    g_debug("setting %i targets", n+1);

    b = gtk_clipboard_set_with_data(primary ? i->primary : i->clipboard, e, n+1, get_func, clear_func, i);
    g_assert(b);
}

void lassi_clipboard_clear(LassiClipboardInfo *i, gboolean primary) {
    g_assert(i);

    gtk_clipboard_clear(primary ? i->primary : i->clipboard);
}

int lassi_clipboard_get(LassiClipboardInfo *i, gboolean primary, const char *target, int *f, gpointer *p, int *l) {
    GtkSelectionData*sd;
    g_assert(i);

    if (!(sd = gtk_clipboard_wait_for_contents(primary ? i->primary : i->clipboard, gdk_atom_intern(target, TRUE))))
        return -1;

    g_assert(gtk_selection_data_get_length(sd) > 0);

    *f = gtk_selection_data_get_format(sd);
    *p = g_memdup(gtk_selection_data_get_data(sd), gtk_selection_data_get_length(sd));
    *l = gtk_selection_data_get_length(sd);

    gtk_selection_data_free(sd);

    return 0;
}
