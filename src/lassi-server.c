#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <libintl.h>

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "lassi-server.h"
#include "lassi-grab.h"
#include "lassi-order.h"
#include "lassi-clipboard.h"
#include "lassi-avahi.h"
#include "lassi-tray.h"

#include "paths.h"

#define LASSI_INTERFACE "org.gnome.MangoLassi"

#define PORT_MIN 7421
#define PORT_MAX (PORT_MIN + 50)

#define CONNECTIONS_MAX 16

static void server_disconnect_all(LassiServer *ls, gboolean clear_order);
static void server_send_update_grab(LassiServer *ls, int y);

static void server_broadcast(LassiServer *ls, DBusMessage *m, LassiConnection *except) {
    GList *i;

    g_assert(ls);
    g_assert(m);

    for (i = ls->connections; i; i = i->next) {
        dbus_bool_t b;
        LassiConnection *lc = i->data;
        DBusMessage *n;

        if (lc == except || !lc->id)
            continue;

        n = dbus_message_copy(m);
        g_assert(n);
        b = dbus_connection_send(lc->dbus_connection, n, NULL);
        g_assert(b);
        dbus_message_unref(n);
    }
}

static void server_layout_changed(LassiServer *ls, int y) {
    g_assert(ls);

    g_debug("updating layout");

    lassi_grab_enable_triggers(&ls->grab_info, !!ls->connections_left, !!ls->connections_right);

    if (ls->active_connection) {
        char *t;
        gboolean to_left = !!g_list_find(ls->connections_left, ls->active_connection);

        t = g_strdup_printf(_("Mouse and keyboard are being redirected to <b>%s</b>, which is located to the <b>%s</b> of this screen.\n"
                            "To redirect input back to this screen, press and release both shift keys simultaneously."),
                            ls->active_connection->id, to_left ? _("left") : _("right"));

        if (to_left)
            lassi_osd_set_text(&ls->osd_info, t, "go-previous", NULL);
        else
            lassi_osd_set_text(&ls->osd_info, t, NULL, "go-next");

        lassi_grab_start(&ls->grab_info, to_left);

    } else {
        lassi_grab_stop(&ls->grab_info, y);
        lassi_osd_hide(&ls->osd_info);
    }
}

void lassi_server_set_order(LassiServer *ls, GList *order) {
    GList *l;
    gboolean on_left = TRUE;
    g_assert(ls);

    lassi_list_free(ls->order);
    ls->order = order;

    g_list_free(ls->connections_left);
    g_list_free(ls->connections_right);

    ls->connections_left = ls->connections_right = NULL;

    for (l = ls->order; l; l = l->next) {
        LassiConnection *lc;

        if (!(lc = g_hash_table_lookup(ls->connections_by_id, l->data))) {

            if (strcmp(ls->id, l->data))
                continue;
        }

        g_assert(lc || on_left);

        if (!lc)
            on_left = FALSE;
        else if (on_left)
            ls->connections_left = g_list_prepend(ls->connections_left, lc);
        else
            ls->connections_right = g_list_prepend(ls->connections_right, lc);
    }

    for (l = ls->connections; l; l = l->next) {
        LassiConnection *lc = l->data;

        if (!lc->id)
            continue;

        if (g_list_find(ls->connections_left, lc))
            continue;

        if (g_list_find(ls->connections_right, lc))
            continue;

        ls->order = g_list_append(ls->order, lc->id);
        ls->connections_right = g_list_prepend(ls->connections_right, lc);
    }

    ls->connections_right = g_list_reverse(ls->connections_right);
    server_layout_changed(ls, -1);

    lassi_prefs_update(&ls->prefs_info);
}

static void server_dump(LassiServer *ls) {
    GList *l;
    int n = 0;

    g_assert(ls);

    g_debug("BEGIN Current connections:");

    g_debug("Displays left of us:");
    for (l = ls->connections_left; l; l = l->next) {
        LassiConnection *lc = l->data;
        if (!lc->id)
            continue;
        g_debug("%2i) %s %s %s", n++, ls->active_connection == lc ? "ACTIVE" : "      ", lc->id, lc->address);
    }

    g_debug("Displays right of us:");
    for (l = ls->connections_right; l; l = l->next) {
        LassiConnection *lc = l->data;
        if (!lc->id)
            continue;
        g_debug("%2i) %s %s %s", n++, ls->active_connection == lc ? "ACTIVE" : "      ", lc->id, lc->address);
    }

    if (!ls->active_connection)
        g_debug("We're the active connection");

    g_debug("END");
}

static void connection_destroy(LassiConnection *lc) {
    g_assert(lc);

    dbus_connection_flush(lc->dbus_connection);
    dbus_connection_close(lc->dbus_connection);
    dbus_connection_unref(lc->dbus_connection);
    g_free(lc->id);
    g_free(lc->address);
    g_free(lc);
}

static void server_pick_active_connection(LassiServer *ls) {
    LassiConnection *pick;
    GList *l;
    char *id;

    pick = NULL;
    id = ls->id;

    for (l = ls->connections; l; l = l->next) {
        LassiConnection *lc = l->data;

        if (!lc->id)
            continue;

        if (strcmp(lc->id, id) > 0) {
            id = lc->id;
            pick = lc;
        }
    }

    ls->active_connection = pick;

    server_send_update_grab(ls, -1);
    server_layout_changed(ls, -1);
}

static void server_send_update_grab(LassiServer *ls, int y) {
    char *active;
    DBusMessage *n;
    dbus_bool_t b;
    gint32 g;

    g_assert(ls);

    active = ls->active_connection ? ls->active_connection->id : ls->id;

    n = dbus_message_new_signal("/", LASSI_INTERFACE, "UpdateGrab");
    g_assert(n);

    g = ++ ls->active_generation;
    b = dbus_message_append_args(
            n,
            DBUS_TYPE_INT32, &g,
            DBUS_TYPE_STRING, &active,
            DBUS_TYPE_INT32, &y,
            DBUS_TYPE_INVALID);
    g_assert(b);

    server_broadcast(ls, n, NULL);
    dbus_message_unref(n);
}

void lassi_server_send_update_order(LassiServer *ls, LassiConnection *except) {
    DBusMessage *n;
    dbus_bool_t b;
    gint32 g;
    DBusMessageIter iter, sub;
    GList *l;

    g_assert(ls);

    n = dbus_message_new_signal("/", LASSI_INTERFACE, "UpdateOrder");
    g_assert(n);

    g = ++ ls->order_generation;
    b = dbus_message_append_args(
            n,
            DBUS_TYPE_INT32, &g,
            DBUS_TYPE_INVALID);
    g_assert(b);

    dbus_message_iter_init_append(n, &iter);

    b = dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &sub);
    g_assert(b);

    for (l = ls->order; l; l = l->next) {
        b = dbus_message_iter_append_basic(&sub, DBUS_TYPE_STRING, &l->data);
        g_assert(b);
    }

    b = dbus_message_iter_close_container(&iter, &sub);
    g_assert(b);

    server_broadcast(ls, n, except);
    dbus_message_unref(n);
}

int lassi_server_change_grab(LassiServer *ls, gboolean to_left, int y) {
    LassiConnection *lc;
    GList *l;

    g_assert(ls);

    l = to_left ? ls->connections_left : ls->connections_right;
    lc = l ? l->data : NULL;

    if (!lc)
        return -1;

    ls->active_connection = lc;

    server_send_update_grab(ls, y);
    server_layout_changed(ls, y);
    return 0;
}

int lassi_server_acquire_grab(LassiServer *ls) {
    g_assert(ls);

    ls->active_connection = NULL;

    server_send_update_grab(ls, -1);
    server_layout_changed(ls, -1);
    return 0;
}

int lassi_server_motion_event(LassiServer *ls, int dx, int dy) {
    DBusMessage *n;
    dbus_bool_t b;

    g_assert(ls);

    if (!ls->active_connection)
        return -1;

    n = dbus_message_new_signal("/", LASSI_INTERFACE, "MotionEvent");
    g_assert(n);

    b = dbus_message_append_args(n, DBUS_TYPE_INT32, &dx, DBUS_TYPE_INT32, &dy, DBUS_TYPE_INVALID);
    g_assert(b);

    b = dbus_connection_send(ls->active_connection->dbus_connection, n, NULL);
    g_assert(b);

    dbus_message_unref(n);

    dbus_connection_flush(ls->active_connection->dbus_connection);

    return 0;
}

int lassi_server_button_event(LassiServer *ls, unsigned button, gboolean is_press) {
    DBusMessage *n;
    dbus_bool_t b;

    if (!ls->active_connection)
        return -1;

    n = dbus_message_new_signal("/", LASSI_INTERFACE, "ButtonEvent");
    g_assert(n);

    b = dbus_message_append_args(n, DBUS_TYPE_UINT32, &button, DBUS_TYPE_BOOLEAN, &is_press, DBUS_TYPE_INVALID);
    g_assert(b);

    b = dbus_connection_send(ls->active_connection->dbus_connection, n, NULL);
    g_assert(b);

    dbus_message_unref(n);

    dbus_connection_flush(ls->active_connection->dbus_connection);

    return 0;
}

int lassi_server_key_event(LassiServer *ls, unsigned key, gboolean is_press) {
    DBusMessage *n;
    dbus_bool_t b;

    if (!ls->active_connection)
        return -1;

    n = dbus_message_new_signal("/", LASSI_INTERFACE, "KeyEvent");
    g_assert(n);

    b = dbus_message_append_args(n, DBUS_TYPE_UINT32, &key, DBUS_TYPE_BOOLEAN, &is_press, DBUS_TYPE_INVALID);
    g_assert(b);

    b = dbus_connection_send(ls->active_connection->dbus_connection, n, NULL);
    g_assert(b);

    dbus_message_unref(n);

    dbus_connection_flush(ls->active_connection->dbus_connection);

    return 0;
}

static void show_welcome(LassiConnection *lc, gboolean is_connect) {
    gboolean to_left;
    LassiServer *ls;
    char *summary, *body;

    g_assert(lc);

    ls = lc->server;
    to_left = !!g_list_find(ls->connections_left, lc);

    if (is_connect) {
        summary = g_strdup_printf(_("%s now shares input with this desktop"), lc->id);
        body = g_strdup_printf(_("You're now sharing keyboard and mouse with <b>%s</b> which is located to the <b>%s</b>."), lc->id, to_left ? _("left") : _("right"));
    } else {
        summary = g_strdup_printf(_("%s no longer shares input with this desktop"), lc->id);
        body = g_strdup_printf(_("You're no longer sharing keyboard and mouse with <b>%s</b> which was located to the <b>%s</b>."), lc->id, to_left ? _("left") : _("right"));
    }

    lassi_tray_show_notification(&ls->tray_info, summary, body, to_left ? LASSI_TRAY_NOTIFICATION_LEFT : LASSI_TRAY_NOTIFICATION_RIGHT);

    g_free(summary);
    g_free(body);
}

static void connection_unlink(LassiConnection *lc, gboolean remove_from_order) {
    LassiServer *ls;
    g_assert(lc);

    g_debug("Unlinking %s (%s)", lc->id, lc->address);

    ls = lc->server;

    if (lc->id) {
        DBusMessage *n;
        dbus_bool_t b;

        /* Tell everyone */
        n = dbus_message_new_signal("/", LASSI_INTERFACE, "NodeRemoved");
        g_assert(n);

        b = dbus_message_append_args(n,
                                     DBUS_TYPE_STRING, &lc->id,
                                     DBUS_TYPE_STRING, &lc->address,
                                     DBUS_TYPE_BOOLEAN, &remove_from_order,
                                     DBUS_TYPE_INVALID);
        g_assert(b);

        server_broadcast(ls, n, NULL);
        dbus_message_unref(n);
    }

    ls->connections = g_list_remove(ls->connections, lc);
    ls->n_connections --;

    if (lc->id) {
        show_welcome(lc, FALSE);

        g_hash_table_remove(ls->connections_by_id, lc->id);
        ls->connections_left = g_list_remove(ls->connections_left, lc);
        ls->connections_right = g_list_remove(ls->connections_right, lc);

        if (ls->active_connection == lc)
            server_pick_active_connection(ls);

        if (ls->clipboard_connection == lc) {
            ls->clipboard_connection = NULL;
            ls->clipboard_empty = TRUE;
            lassi_clipboard_clear(&lc->server->clipboard_info, FALSE);
        }

        if (ls->primary_connection == lc) {
            ls->primary_connection = NULL;
            ls->primary_empty = TRUE;
            lassi_clipboard_clear(&lc->server->clipboard_info, TRUE);
        }

        if (remove_from_order) {
            GList *i = g_list_find_custom(ls->order, lc->id, (GCompareFunc) strcmp);

            if (i)
                ls->order = g_list_delete_link(ls->order, i);
        }

        server_layout_changed(ls, -1);
        lassi_prefs_update(&ls->prefs_info);
        server_dump(ls);
    }

    lassi_tray_update(&ls->tray_info, ls->n_connections);

    connection_destroy(lc);
}

static void server_position_connection(LassiServer *ls, LassiConnection *lc) {
    GList *l;
    LassiConnection *last = NULL;

    g_assert(ls);
    g_assert(lc);

    g_assert(!g_list_find(ls->connections_left, lc));
    g_assert(!g_list_find(ls->connections_right, lc));

    for (l = ls->order; l; l = l->next) {
        LassiConnection *k;

        if (strcmp(l->data, lc->id) == 0)
            break;

        if ((k = g_hash_table_lookup(ls->connections_by_id, l->data)))
            last = k;
    }

    if (l) {
        /* OK, We found a spot to add this */

        if (last) {
            GList *j;

            /*Ok, this one belongs to the right of 'last' */

            if ((j = g_list_find(ls->connections_left, last)))
                /* This one belongs in the left list */
                ls->connections_left = g_list_insert_before(ls->connections_left, j, lc);
            else {
                /* This one belongs in the rightlist */
                ls->connections_right = g_list_reverse(ls->connections_right);
                j = g_list_find(ls->connections_right, last);
                g_assert(j);
                ls->connections_right = g_list_insert_before(ls->connections_right, j, lc);
                ls->connections_right = g_list_reverse(ls->connections_right);
            }
        } else
            /* Hmm, this is before the left end */
            ls->connections_left = g_list_append(ls->connections_left, lc);
    } else {
        ls->order = g_list_append(ls->order, g_strdup(lc->id));
        /* No spot found, let's add it to the right end */
        ls->connections_right = g_list_append(ls->connections_right, lc);
    }
}

int lassi_server_acquire_clipboard(LassiServer *ls, gboolean primary, char**targets) {
    DBusMessageIter iter, sub;
    DBusMessage *n;
    gint32 g;
    gboolean b;

    g_assert(ls);
    g_assert(targets);

    if (primary) {
        ls->primary_empty = FALSE;
        ls->primary_connection = NULL;
    } else {
        ls->clipboard_empty = FALSE;
        ls->clipboard_connection = NULL;
    }

    n = dbus_message_new_signal("/", LASSI_INTERFACE, "AcquireClipboard");
    g_assert(n);

    if (primary)
        g = ++ ls->primary_generation;
    else
        g = ++ ls->clipboard_generation;

    b = dbus_message_append_args(n, DBUS_TYPE_INT32, &g, DBUS_TYPE_BOOLEAN, &primary, DBUS_TYPE_INVALID);
    g_assert(b);

    dbus_message_iter_init_append(n, &iter);

    b = dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &sub);
    g_assert(b);

    for (; *targets; targets++) {
        g_debug("Exporting target %s", *targets);
        b = dbus_message_iter_append_basic(&sub, DBUS_TYPE_STRING, targets);
        g_assert(b);
    }

    b = dbus_message_iter_close_container(&iter, &sub);
    g_assert(b);

    server_broadcast(ls, n, NULL);
    g_assert(b);

    dbus_message_unref(n);
    return 0;
}

int lassi_server_return_clipboard(LassiServer *ls, gboolean primary) {
    DBusMessage *n;
    guint32 g;
    gboolean b;

    g_assert(ls);

    if (primary) {

        if (ls->primary_empty || ls->primary_connection != NULL)
            return -1;

        ls->primary_empty = TRUE;
        ls->primary_connection = NULL;

    } else {

        if (ls->clipboard_empty || ls->clipboard_connection != NULL)
            return -1;

        ls->clipboard_empty = TRUE;
        ls->clipboard_connection = NULL;
    }

    n = dbus_message_new_signal("/", LASSI_INTERFACE, "ReturnClipboard");
    g_assert(n);

    if (primary)
        g = ++ ls->primary_generation;
    else
        g = ++ ls->clipboard_generation;

    b = dbus_message_append_args(n, DBUS_TYPE_UINT32, &g, DBUS_TYPE_BOOLEAN, &primary, DBUS_TYPE_INVALID);
    g_assert(b);

    server_broadcast(ls, n, NULL);

    dbus_message_unref(n);
    return 0;
}

int lassi_server_get_clipboard(LassiServer *ls, gboolean primary, const char *t, int *f, gpointer *p, int *l) {
    DBusMessage *n, *reply;
    DBusConnection *c;
    DBusError e;
    int ret = -1;
    DBusMessageIter iter, sub;
    gboolean b;

    g_assert(ls);

    dbus_error_init(&e);

    if (primary) {

        if (ls->primary_empty || !ls->primary_connection)
            return -1;

        c = ls->primary_connection->dbus_connection;

    } else {

        if (ls->clipboard_empty || !ls->clipboard_connection)
            return -1;

        c = ls->clipboard_connection->dbus_connection;
    }

    n = dbus_message_new_method_call(NULL, "/", LASSI_INTERFACE, "GetClipboard");
    g_assert(n);

    b = dbus_message_append_args(n, DBUS_TYPE_BOOLEAN, &primary, DBUS_TYPE_STRING, &t, DBUS_TYPE_INVALID);
    g_assert(b);

    if (!(reply = dbus_connection_send_with_reply_and_block(c, n, -1, &e))) {
        g_debug("Getting clipboard failed: %s", e.message);
        goto finish;
    }

    dbus_message_iter_init(reply, &iter);
    dbus_message_iter_get_basic(&iter, f);
    dbus_message_iter_next(&iter);

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY || dbus_message_iter_get_element_type(&iter) != DBUS_TYPE_BYTE) {
        g_debug("Invalid clipboard data");
        goto finish;
    }

    dbus_message_iter_recurse(&iter, &sub);
    dbus_message_iter_get_fixed_array(&sub, p, l);

    *p = g_memdup(*p, *l);

    ret = 0;

finish:

    dbus_message_unref(n);

    if (reply)
        dbus_message_unref(reply);

    dbus_error_free(&e);

    return ret;
}

static int signal_hello(LassiConnection *lc, DBusMessage *m) {
    const char *id, *address;
    DBusError e;
    GList *i;
    dbus_bool_t b;
    DBusMessage *n;
    gint32 active_generation, order_generation, clipboard_generation;

    dbus_error_init(&e);

    if (lc->id) {
        g_debug("Received duplicate HelloNode.");
        return -1;
    }

    if (!(dbus_message_get_args(
                  m, &e,
                  DBUS_TYPE_STRING, &id,
                  DBUS_TYPE_STRING, &address,
                  DBUS_TYPE_INT32, &active_generation,
                  DBUS_TYPE_INT32, &order_generation,
                  DBUS_TYPE_INT32, &clipboard_generation,
                  DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

    if (strcmp(id, lc->server->id) == 0) {
        g_debug("Dropping looped back connection.");
        return -1;
    }

    if (g_hash_table_lookup(lc->server->connections_by_id, id)) {
        g_debug("Dropping duplicate connection.");
        return -1;
    }

    lc->server->active_generation = MAX(lc->server->active_generation, active_generation);
    lc->server->order_generation = MAX(lc->server->order_generation, order_generation);
    lc->server->clipboard_generation = MAX(lc->server->clipboard_generation, clipboard_generation);

    g_debug("Got welcome from %s (%s)", id, address);

    lc->id = g_strdup(id);
    lc->address = g_strdup(address);
    g_hash_table_insert(lc->server->connections_by_id, lc->id, lc);
    server_position_connection(lc->server, lc);

    /* Notify all old nodes of the new one */
    n = dbus_message_new_signal("/", LASSI_INTERFACE, "NodeAdded");
    g_assert(n);

    b = dbus_message_append_args(n, DBUS_TYPE_STRING, &id, DBUS_TYPE_STRING, &address, DBUS_TYPE_INVALID);
    g_assert(b);

    server_broadcast(lc->server, n, lc);
    dbus_message_unref(n);

    /* Notify new node about old nodes */
    for (i = lc->server->connections; i; i = i->next) {
        LassiConnection *k = i->data;

        if (k == lc || !k->id)
            continue;

        n = dbus_message_new_signal("/", LASSI_INTERFACE, "NodeAdded");
        g_assert(n);

        b = dbus_message_append_args(n, DBUS_TYPE_STRING, &id, DBUS_TYPE_STRING, &address, DBUS_TYPE_INVALID);
        g_assert(b);

        b = dbus_connection_send(lc->dbus_connection, n, NULL);
        g_assert(b);

        dbus_message_unref(n);
    }

    if (lc->we_are_client) {
        server_send_update_grab(lc->server, -1);
        lassi_server_send_update_order(lc->server, NULL);

        lc->delayed_welcome = FALSE;
        show_welcome(lc, TRUE);
    } else
        lc->delayed_welcome = TRUE;

    server_layout_changed(lc->server, -1);
    lassi_prefs_update(&lc->server->prefs_info);

    server_dump(lc->server);

    return 0;
}

static int signal_node_added(LassiConnection *lc, DBusMessage *m) {
    const char *id, *address;
    DBusError e;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(m, &e, DBUS_TYPE_STRING, &id, DBUS_TYPE_STRING, &address, DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

    if (strcmp(id, lc->server->id) == 0)
        return 0;

    if (g_hash_table_lookup(lc->server->connections_by_id, id))
        return 0;

    if (!(lassi_server_connect(lc->server, address))) {
        DBusMessage *n;
        dbus_bool_t b;

        /* Failed to connnect to this client, tell everyone */
        n = dbus_message_new_signal("/", LASSI_INTERFACE, "NodeRemoved");
        g_assert(n);

        b = dbus_message_append_args(n, DBUS_TYPE_STRING, &id, DBUS_TYPE_STRING, &address, DBUS_TYPE_INVALID);
        g_assert(b);

        server_broadcast(lc->server, n, NULL);
        dbus_message_unref(n);
    }

    return 0;
}

static int signal_node_removed(LassiConnection *lc, DBusMessage *m) {
    const char *id, *address;
    DBusError e;
    LassiConnection *k;
    gboolean remove_from_order;
    LassiServer *ls = lc->server;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(m, &e,
                                DBUS_TYPE_STRING, &id,
                                DBUS_TYPE_STRING, &address,
                                DBUS_TYPE_BOOLEAN, &remove_from_order,
                                DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

    if (strcmp(id, lc->server->id) == 0) {
        g_debug("We've been kicked ourselves.");

        server_disconnect_all(lc->server, TRUE);
        return 0;
    }

    if (remove_from_order) {
        GList *i = g_list_find_custom(ls->order, id, (GCompareFunc) strcmp);

        if (i)
            ls->order = g_list_delete_link(ls->order, i);
    }

    if ((k = g_hash_table_lookup(lc->server->connections_by_id, id)))
        connection_unlink(k, remove_from_order);

    server_broadcast(ls, m, lc == k ? NULL : lc);

    return 0;
}

static int signal_update_grab(LassiConnection *lc, DBusMessage *m) {
    const char*id, *current_id;
    gint32 generation;
    LassiConnection *k = NULL;
    DBusError e;
    int y;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(
                  m, &e,
                  DBUS_TYPE_INT32, &generation,
                  DBUS_TYPE_STRING, &id,
                  DBUS_TYPE_INT32, &y,
                  DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

    g_debug("received grab request for %s (%i vs %i)", id, lc->server->active_generation, generation);

    if (strcmp(id, lc->server->id) && !(k = g_hash_table_lookup(lc->server->connections_by_id, id))) {
        g_debug("Unknown connection");
        return -1;
    }

    if (k == lc->server->active_connection) {
        g_debug("Connection already active");
        return 0;
    }

    current_id = lc->server->active_connection ? lc->server->active_connection->id : lc->server->id;

    if ((lc->server->active_generation > generation || (lc->server->active_generation == generation && strcmp(current_id, id) > 0))) {
        g_debug("Ignoring request for active connection");
        return 0;
    }

    lc->server->active_connection = k;
    lc->server->active_generation = generation;

    if (!k)
        g_debug("We're now the active server.");
    else
        g_debug("Connection '%s' activated.", k->id);

    server_broadcast(lc->server, m, lc);
    server_layout_changed(lc->server, y);

    return 0;
}

static int signal_update_order(LassiConnection *lc, DBusMessage *m) {
    gint32 generation;
    DBusError e;
    DBusMessageIter iter, sub;
    GList *new_order = NULL, *merged_order = NULL;
    int r = 0;
    int c = 0;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(
                  m, &e,
                  DBUS_TYPE_INT32, &generation,
                  DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

    dbus_message_iter_init(m, &iter);
    dbus_message_iter_next(&iter);

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY || dbus_message_iter_get_element_type(&iter) != DBUS_TYPE_STRING) {
        g_debug("Bad connection list fo the left");
        return -1;
    }

    if (lc->server->order_generation > generation) {
        g_debug("Ignoring request for layout");
        return 0;
    }

    dbus_message_iter_recurse(&iter, &sub);

    while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
        const char *id;
        dbus_message_iter_get_basic(&sub, &id);
        new_order = g_list_prepend(new_order, g_strdup(id));
        dbus_message_iter_next(&sub);
    }

    new_order = g_list_reverse(new_order);

    if (!lassi_list_nodups(new_order)) {
        g_debug("Received invalid list.");
        r = -1;
        goto finish;
    }

    c = lassi_list_compare(lc->server->order, new_order);

    if (c == 0) {
        g_debug("Requested order identical to ours.");
        goto finish;
    }

    if (lc->server->order_generation == generation &&  c > 0) {
        g_debug("Ignoring request for layout 2");
        goto finish;
    }

    merged_order = lassi_list_merge(lassi_list_copy(new_order), lc->server->order);

    if (lassi_list_compare(lc->server->order, merged_order)) {
        lassi_server_set_order(lc->server, merged_order);
        merged_order = NULL;
    }

    lassi_server_send_update_order(lc->server, lassi_list_compare(lc->server->order, new_order) ? NULL : lc);

    lc->server->order_generation = generation;

finish:

    lassi_list_free(new_order);
    lassi_list_free(merged_order);

    if (lc->delayed_welcome) {
        lc->delayed_welcome = FALSE;
        show_welcome(lc, TRUE);
    }

    return r;
}

static int signal_key_event(LassiConnection *lc, DBusMessage *m) {
    DBusError e;
    guint32 key;
    gboolean is_press;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(m, &e, DBUS_TYPE_UINT32, &key, DBUS_TYPE_BOOLEAN, &is_press, DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

/*     g_debug("got dbus key %i %i", key, !!is_press); */
    lassi_grab_press_key(&lc->server->grab_info, key, is_press);

    return 0;
}

static int signal_motion_event(LassiConnection *lc, DBusMessage *m) {
    DBusError e;
    int dx, dy;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(m, &e, DBUS_TYPE_INT32, &dx, DBUS_TYPE_INT32, &dy, DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

/*     g_debug("got dbus motion %i %i", dx, dy); */
    lassi_grab_move_pointer_relative(&lc->server->grab_info, dx, dy);

    return 0;
}

static int signal_button_event(LassiConnection *lc, DBusMessage *m) {
    DBusError e;
    guint32 button;
    gboolean is_press;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(m, &e, DBUS_TYPE_UINT32, &button, DBUS_TYPE_BOOLEAN, &is_press, DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

/*     g_debug("got dbus button %i %i", button, !!is_press); */
    lassi_grab_press_button(&lc->server->grab_info, button, is_press);

    return 0;
}

static int signal_acquire_clipboard(LassiConnection *lc, DBusMessage *m) {
    DBusError e;
    gint32 g;
    gboolean primary;
    DBusMessageIter iter, sub;
    char **targets;
    unsigned alloc_targets, j;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(m, &e, DBUS_TYPE_INT32, &g, DBUS_TYPE_BOOLEAN, &primary, DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

    if ((primary ? lc->server->primary_generation : lc->server->clipboard_generation) > g) {
        g_debug("Ignoring request for clipboard.");
        return 0;
    }

    /* FIXME, tie break missing */

    dbus_message_iter_init(m, &iter);
    dbus_message_iter_next(&iter);
    dbus_message_iter_next(&iter);

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY || dbus_message_iter_get_element_type(&iter) != DBUS_TYPE_STRING) {
        g_debug("Bad target list");
        return -1;
    }

    dbus_message_iter_recurse(&iter, &sub);

    targets = g_new(char*, alloc_targets = 20);
    j = 0;

    while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
        const char *t;
        dbus_message_iter_get_basic(&sub, &t);

        if (j >= alloc_targets) {
            alloc_targets *= 2;
            targets = g_realloc(targets, sizeof(char*) * (alloc_targets+1));
        }

        g_assert(j < alloc_targets);

        targets[j++] = (char*) t;
        dbus_message_iter_next(&sub);

        g_debug("Received target %s on %s", t, lc->id);
    }

    targets[j] = NULL;

    lassi_clipboard_set(&lc->server->clipboard_info, primary, targets);

    g_free(targets);

    if (primary) {
        lc->server->primary_connection = lc;
        lc->server->primary_empty = FALSE;
        lc->server->primary_generation = g;
    } else {
        lc->server->clipboard_connection = lc;
        lc->server->clipboard_empty = FALSE;
        lc->server->clipboard_generation = g;
    }

    return 0;
}

static int signal_return_clipboard(LassiConnection *lc, DBusMessage *m) {
    DBusError e;
    gint32 g;
    gboolean primary;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(m, &e, DBUS_TYPE_INT32, &g, DBUS_TYPE_BOOLEAN, &primary, DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

    if ((primary ? lc->server->primary_generation : lc->server->clipboard_generation) > g) {
        g_debug("Ignoring request for clipboard empty.");
        return 0;
    }

    /* FIXME, tie break missing */

    lassi_clipboard_clear(&lc->server->clipboard_info, primary);

    if (primary) {
        lc->server->primary_connection = NULL;
        lc->server->primary_empty = TRUE;
        lc->server->primary_generation = g;
    } else {
        lc->server->clipboard_connection = NULL;
        lc->server->clipboard_empty = TRUE;
        lc->server->clipboard_generation = g;
    }

    return 0;
}

static int method_get_clipboard(LassiConnection *lc, DBusMessage *m) {
    DBusError e;
    char *type;
    gboolean primary;
    DBusMessage *n = NULL;
    gint32 f;
    gpointer p = NULL;
    int l = 0;
    DBusMessageIter iter, sub;
    gboolean b;

    dbus_error_init(&e);

    if (!(dbus_message_get_args(m, &e, DBUS_TYPE_BOOLEAN, &primary, DBUS_TYPE_STRING, &type, DBUS_TYPE_INVALID))) {
        g_debug("Received invalid message: %s", e.message);
        dbus_error_free(&e);
        return -1;
    }

    if ((primary && (lc->server->primary_connection || lc->server->primary_empty)) ||
        (!primary && (lc->server->clipboard_connection || lc->server->clipboard_empty))) {
        n = dbus_message_new_error(m, LASSI_INTERFACE ".NotOwner", "We're not the clipboard owner");
        goto finish;
    }

    if (lassi_clipboard_get(&lc->server->clipboard_info, primary, type, &f, &p, &l) < 0) {
        n = dbus_message_new_error(m, LASSI_INTERFACE ".ClipboardFailure", "Failed to read clipboard data");
        goto finish;
    }

    if (l > dbus_connection_get_max_message_size(lc->dbus_connection)*9/10) {
        n = dbus_message_new_error(m, LASSI_INTERFACE ".TooLarge", "Clipboard data too large");
        goto finish;
    }

    n = dbus_message_new_method_return(m);
    g_assert(n);

    dbus_message_iter_init_append(n, &iter);
    b = dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &f);
    g_assert(b);

    b = dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &sub);
    g_assert(b);

    b = dbus_message_iter_append_fixed_array(&sub, DBUS_TYPE_BYTE, &p, l);
    g_assert(b);

    b = dbus_message_iter_close_container(&iter, &sub);
    g_assert(b);

finish:
    g_assert(n);

    dbus_connection_send(lc->dbus_connection, n, NULL);
    dbus_message_unref(n);

    g_free(p);

    return 0;
}

static DBusHandlerResult message_function(DBusConnection *c, DBusMessage *m, void *userdata) {
    DBusError e;
    LassiConnection *lc = userdata;

    g_assert(c);
    g_assert(m);
    g_assert(lc);

    dbus_error_init(&e);

/*     g_debug("[%s] interface=%s, path=%s, member=%s serial=%u", */
/*             lc->id, */
/*             dbus_message_get_interface(m), */
/*             dbus_message_get_path(m), */
/*             dbus_message_get_member(m), */
/*             dbus_message_get_serial(m)); */

    if (dbus_message_is_signal(m, DBUS_INTERFACE_LOCAL, "Disconnected"))
        goto fail;

    else if (dbus_message_is_signal(m, LASSI_INTERFACE, "Hello")) {
        if (signal_hello(lc, m) < 0)
            goto fail;

    } else if (lc->id) {

        if (dbus_message_is_signal(m, LASSI_INTERFACE, "NodeAdded")) {

            if (signal_node_added(lc, m) < 0)
                goto fail;

        } else if (dbus_message_is_signal(m, LASSI_INTERFACE, "NodeRemoved")) {

            if (signal_node_removed(lc, m) < 0)
                goto fail;

        } else if (dbus_message_is_signal(m, LASSI_INTERFACE, "UpdateGrab")) {

            if (signal_update_grab(lc, m) < 0)
                goto fail;

        } else if (dbus_message_is_signal(m, LASSI_INTERFACE, "UpdateOrder")) {

            if (signal_update_order(lc, m) < 0)
                goto fail;

        } else if (dbus_message_is_signal(m, LASSI_INTERFACE, "KeyEvent")) {

            if (signal_key_event(lc, m) < 0)
                goto fail;

        } else if (dbus_message_is_signal(m, LASSI_INTERFACE, "MotionEvent")) {

            if (signal_motion_event(lc, m) < 0)
                goto fail;

        } else if (dbus_message_is_signal(m, LASSI_INTERFACE, "ButtonEvent")) {

            if (signal_button_event(lc, m) < 0)
                goto fail;

        } else if (dbus_message_is_signal(m, LASSI_INTERFACE, "AcquireClipboard")) {

            if (signal_acquire_clipboard(lc, m) < 0)
                goto fail;

        } else if (dbus_message_is_signal(m, LASSI_INTERFACE, "ReturnClipboard")) {

            if (signal_return_clipboard(lc, m) < 0)
                goto fail;

        } else if (dbus_message_is_method_call(m, LASSI_INTERFACE, "GetClipboard")) {

            if (method_get_clipboard(lc, m) < 0)
                goto fail;

        } else
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    } else
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    return DBUS_HANDLER_RESULT_HANDLED;

fail:

    dbus_error_free(&e);

    connection_unlink(lc, TRUE);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static LassiConnection* connection_add(LassiServer *ls, DBusConnection *c, gboolean we_are_client) {
    LassiConnection *lc;
    dbus_bool_t b;
    DBusMessage *m;
    gint32 ag, og, cg;
    int fd, one = 1;

    g_assert(ls);
    g_assert(c);

    lc = g_new(LassiConnection, 1);
    lc->dbus_connection = dbus_connection_ref(c);
    lc->server = ls;
    lc->id = lc->address = NULL;
    lc->we_are_client = we_are_client;
    lc->delayed_welcome = FALSE;
    ls->connections = g_list_prepend(ls->connections, lc);
    ls->n_connections++;

    dbus_connection_setup_with_g_main(c, NULL);

    b = dbus_connection_add_filter(c, message_function, lc, NULL);
    g_assert(b);

    m = dbus_message_new_signal("/", LASSI_INTERFACE, "Hello");
    g_assert(m);

    ag = ls->active_generation;
    og = ls->order_generation;
    cg = ls->clipboard_generation;

    b = dbus_message_append_args(
            m,
            DBUS_TYPE_STRING, &ls->id,
            DBUS_TYPE_STRING, &ls->address,
            DBUS_TYPE_INT32, &ag,
            DBUS_TYPE_INT32, &og,
            DBUS_TYPE_INT32, &cg,
            DBUS_TYPE_INVALID);
    g_assert(b);

    fd = -1;
    dbus_connection_get_socket(c, &fd);
    g_assert(fd >= 0);

    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one)) < 0)
        g_warning("Failed to enable TCP_NODELAY");

    b = dbus_connection_send(c, m, NULL);
    g_assert(b);

    dbus_message_unref(m);

    lassi_tray_update(&ls->tray_info, ls->n_connections);
    return lc;
}

static void new_connection(DBusServer *s, DBusConnection *c, void *userdata) {
    LassiServer *ls = userdata;

    g_assert(s);
    g_assert(c);

    if (ls->n_connections >= CONNECTIONS_MAX)
        return;

    dbus_connection_set_allow_anonymous(c, TRUE);
    connection_add(ls, c, FALSE);
}

static int server_init(LassiServer *ls) {
    DBusError e;
    int r = -1;
    guint16 port;

    g_assert(ls);

    memset(ls, 0, sizeof(*ls));

    dbus_error_init(&e);

    for (port = PORT_MIN; port < PORT_MAX; port++) {
        char *t;

        t = g_strdup_printf("tcp:port=%u,host=0.0.0.0", port);
        ls->dbus_server = dbus_server_listen(t, &e);
        g_free(t);

        if (ls->dbus_server) {
            ls->port = port;
            break;
        }

        if (!dbus_error_has_name(&e, DBUS_ERROR_ADDRESS_IN_USE)) {
            g_warning("Failed to create D-Bus server: %s %s", e.message, e.name);
            goto finish;
        }

        dbus_error_free(&e);
    }

    if (!ls->dbus_server) {
        g_warning("All ports blocked.");
        goto finish;
    }

    g_debug("Listening on port %u", port);

    dbus_server_setup_with_g_main(ls->dbus_server, NULL);
    dbus_server_set_new_connection_function(ls->dbus_server, new_connection, ls, NULL);

    ls->connections_by_id = g_hash_table_new(g_str_hash, g_str_equal);

    ls->id = g_strdup_printf(_("%s's desktop on %s"), g_get_user_name(), g_get_host_name());

    if (lassi_avahi_init(&ls->avahi_info, ls) < 0)
        goto finish;

    /* The initialization of Avahi might have changed ls->id! */

    ls->address = dbus_server_get_address(ls->dbus_server);
    ls->order = g_list_prepend(NULL, g_strdup(ls->id));

    if (lassi_grab_init(&ls->grab_info, ls) < 0)
        goto finish;

    if (lassi_osd_init(&ls->osd_info) < 0)
        goto finish;

    if (lassi_tray_init(&ls->tray_info, ls) < 0)
        goto finish;

    if (lassi_clipboard_init(&ls->clipboard_info, ls) < 0)
        goto finish;

    if (lassi_prefs_init(&ls->prefs_info, ls) < 0)
        goto finish;

    r = 0;

finish:
    dbus_error_free(&e);
    return r;
}

void lassi_server_disconnect(LassiServer *ls, const char *id, gboolean remove_from_order) {
    LassiConnection *lc;

    g_assert(ls);
    g_assert(id);

    if ((lc = g_hash_table_lookup(ls->connections_by_id, id)))
        connection_unlink(lc, remove_from_order);
    else if (remove_from_order) {
        GList *i = g_list_find_custom(ls->order, id, (GCompareFunc) strcmp);

        if (i)
            ls->order = g_list_delete_link(ls->order, i);
    }
}

static void server_disconnect_all(LassiServer *ls, gboolean clear_order) {

    while (ls->connections)
        connection_unlink(ls->connections->data, clear_order);

    if (clear_order) {
        lassi_list_free(ls->order);
        ls->order = NULL;
    }
}

static void server_done(LassiServer *ls) {

    g_assert(ls);

    if (ls->dbus_server) {
        dbus_server_disconnect(ls->dbus_server);
        dbus_server_unref(ls->dbus_server);
    }

    server_disconnect_all(ls, FALSE);

    if (ls->connections_by_id)
        g_hash_table_destroy(ls->connections_by_id);

    g_free(ls->id);
    g_free(ls->address);

    lassi_list_free(ls->order);

    lassi_grab_done(&ls->grab_info);
    lassi_osd_done(&ls->osd_info);
    lassi_clipboard_done(&ls->clipboard_info);
    lassi_avahi_done(&ls->avahi_info);
    lassi_tray_done(&ls->tray_info);
    lassi_prefs_done(&ls->prefs_info);

    memset(ls, 0, sizeof(*ls));
}

gboolean lassi_server_is_connected(LassiServer *ls, const char *id) {
    g_assert(ls);
    g_assert(id);

    return strcmp(id, ls->id) == 0 || g_hash_table_lookup(ls->connections_by_id, id);
}

gboolean lassi_server_is_known(LassiServer *ls, const char *id) {
    g_assert(ls);
    g_assert(id);

    return !!g_list_find_custom(ls->order, id, (GCompareFunc) strcmp);
}

LassiConnection* lassi_server_connect(LassiServer *ls, const char *a) {
    DBusError e;
    DBusConnection *c = NULL;
    LassiConnection *lc = NULL;

    dbus_error_init(&e);

    if (ls->n_connections >= CONNECTIONS_MAX)
        goto finish;

    if (!(c = dbus_connection_open_private(a, &e))) {
        g_warning("Failed to connect to client: %s", e.message);
        goto finish;
    }

    lc = connection_add(ls, c, TRUE);

finish:

    if (c)
        dbus_connection_unref(c);

    dbus_error_free(&e);
    return lc;
}

static void log_handler(gchar const* log_domain, GLogLevelFlags log_level, gchar const* message, gpointer user_data) {
    gboolean* verbose = user_data;

    if (!*verbose && log_level > G_LOG_LEVEL_MESSAGE)
        return;

    g_log_default_handler (log_domain, log_level, message, NULL);
}

int main(int argc, char *argv[]) {
    gboolean verbose = FALSE;
    GOptionEntry  entries[] = {
        {
            "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
            N_("display information useful for debugging"), NULL
        },
        {NULL, 0, 0, 0, NULL, NULL, NULL}
    };
    LassiServer ls;
    GError     *error = NULL;

    /* workaround bug-buddy using our logging handler in an unsave way
     * http://github.com/herzi/mango-lassi/issues/#issue/1
     * and
     * http://bugs.gnome.org/622068 */
    g_setenv ("GNOME_DISABLE_CRASH_DIALOG", "1", TRUE);

    /* Initialize the i18n stuff */
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    g_log_set_default_handler (log_handler, &verbose);

    if (!gtk_init_with_args(&argc, &argv, NULL, entries, NULL, &error)) {
        g_warning ("error while parsing the command line arguments%s%s",
                   error ? ": " : "",
                   error ? error->message : "");

        g_clear_error (&error);
        return 1;
    }
    /* FIXME: try setting the application name from the startup information */
    // g_get_application_name ();
    gtk_window_set_default_icon_name (g_get_prgname ());

    memset(&ls, 0, sizeof(ls));

    if (server_init(&ls) < 0)
        goto fail;

    gtk_main();

fail:

    server_done(&ls);

    return 0;
}
