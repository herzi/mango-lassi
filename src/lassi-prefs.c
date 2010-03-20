#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <avahi-ui/avahi-ui.h>

#include "lassi-prefs.h"
#include "lassi-server.h"

#include "paths.h"

#ifndef GLADE_FILE
#define GLADE_FILE "mango-lassi.glade"
#endif

enum {
    COLUMN_ICON,
    COLUMN_NAME,
    COLUMN_GLIST,
    N_COLUMNS
};

static void update_sensitive(LassiPrefsInfo *i);

static void on_add_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
    GtkWidget *d;

    d = aui_service_dialog_new("Choose Desktop to add", GTK_WINDOW(i->dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT, NULL);
    aui_service_dialog_set_browse_service_types(AUI_SERVICE_DIALOG(d), LASSI_SERVICE_TYPE, NULL);

    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char a[AVAHI_ADDRESS_STR_MAX], *t;

        avahi_address_snprint(a, sizeof(a), aui_service_dialog_get_address(AUI_SERVICE_DIALOG(d)));
        t = g_strdup_printf("tcp:port=%u,host=%s", aui_service_dialog_get_port(AUI_SERVICE_DIALOG(d)), a);
        lassi_server_connect(i->server,  t);
        g_free(t);
    }

    gtk_widget_destroy(d);
}

static void on_remove_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
    GtkTreeIter iter;
    GList* selected;
    char *id;

    selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(i->icon_view));

    if (!selected || !gtk_tree_model_get_iter(GTK_TREE_MODEL(i->list_store), &iter, selected->data))
        return;

    gtk_tree_model_get(GTK_TREE_MODEL(i->list_store), &iter, COLUMN_NAME, &id, -1);
    if (id) {
        lassi_server_disconnect(i->server, id, TRUE);
        g_free(id);
    }

    g_list_foreach(selected, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(selected);
}

static void on_up_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
    /* FIXME: memory management is broken; this function is _really_ leaky */
    GtkTreeModel *model = GTK_TREE_MODEL(i->list_store); /* FIXME: remove local variable */
    GtkTreeIter iter;
    GtkTreeIter prev;
    GtkTreePath *path;
    GList *selected;
    GList *o = NULL;

    selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(i->icon_view));

    if (!selected || !gtk_tree_model_get_iter(model, &iter, selected->data))
        return;

    path = selected->data;
    if (!gtk_tree_path_prev(path))
        return;

    if (!gtk_tree_model_get_iter(model, &prev, path))
        return;

    gtk_list_store_swap(GTK_LIST_STORE(model), &iter, &prev);

    update_sensitive(i);

    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    do {
        GValue id = {0};
        char *c;
        gtk_tree_model_get_value(model, &iter, COLUMN_NAME, &id);
        c = g_value_dup_string(&id);
        o = g_list_append(o, c);
        g_value_unset(&id);
    } while (gtk_tree_model_iter_next(model, &iter));

    // set_order steals the order list
    lassi_server_set_order(i->server, o);
    lassi_server_send_update_order(i->server, i->server->active_connection);

    g_list_foreach(selected, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(selected);
}

static void on_down_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
    /* FIXME: memory management is broken; this function is _really_ leaky */
    GtkTreeModel *model = GTK_TREE_MODEL(i->list_store); /* FIXME: remove local variable */
    GtkTreeIter iter;
    GtkTreeIter *next;
    GList *selected;
    GList *o = NULL;

    selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(i->icon_view));

    if (!selected || !gtk_tree_model_get_iter(model, &iter, selected->data))
        return;

    /* FIXME: don't allocate memory for that */
    next = gtk_tree_iter_copy(&iter);
    if (!gtk_tree_model_iter_next(model, next))
        return;

    gtk_list_store_swap(GTK_LIST_STORE(model), &iter, next);
    gtk_tree_iter_free(next);

    update_sensitive(i);

    if (!gtk_tree_model_get_iter_first(model, &iter))
        return;

    do {
        GValue id = {0};
        char *c;
        gtk_tree_model_get_value(model, &iter, COLUMN_NAME, &id);
        c = g_value_dup_string(&id);
        o = g_list_append(o, c);
        g_value_unset(&id);
    } while (gtk_tree_model_iter_next(model, &iter));

    // set_order steals the order list
    lassi_server_set_order(i->server, o);
    lassi_server_send_update_order(i->server, i->server->active_connection);

    g_list_foreach(selected, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(selected);
}

static void on_close_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
    gtk_widget_hide(GTK_WIDGET(i->dialog));
}

static void update_sensitive(LassiPrefsInfo *i) {
    GtkTreeIter iter;
    GtkTreePath *path;
    gboolean is_first;
    char *id;
    GList *selected;

    selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(i->icon_view));

    if (!selected || !gtk_tree_model_get_iter(GTK_TREE_MODEL(i->list_store), &iter, selected->data)) {
        gtk_widget_set_sensitive(i->up_button, FALSE);
        gtk_widget_set_sensitive(i->down_button, FALSE);
        gtk_widget_set_sensitive(i->remove_button, FALSE);
        return;
    }

    gtk_tree_model_get(GTK_TREE_MODEL(i->list_store), &iter, COLUMN_NAME, &id, -1);
    gtk_widget_set_sensitive(i->remove_button, strcmp(id, i->server->id) != 0);
    g_free(id);

    path = selected->data;

    is_first = gtk_tree_path_prev(path);
    gtk_widget_set_sensitive(i->up_button, is_first);
    if (is_first)
        gtk_tree_path_next(path);

    gtk_tree_path_next(path);
    gtk_widget_set_sensitive(i->down_button, gtk_tree_model_get_iter(GTK_TREE_MODEL(i->list_store), &iter, path));

    g_list_foreach(selected, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(selected);
}

static void on_selection_changed(GtkIconView *icon_view, LassiPrefsInfo *i) {
    update_sensitive(i);
}

int lassi_prefs_init(LassiPrefsInfo *i, LassiServer *server) {
    g_assert(i);
    g_assert(server);

    memset(i, 0, sizeof(*i));
    i->server = server;

    i->xml = glade_xml_new(GLADE_FILE, NULL, NULL);

    i->dialog = glade_xml_get_widget(i->xml, "preferences_dialog");
    i->up_button = glade_xml_get_widget(i->xml, "up_button");
    i->down_button = glade_xml_get_widget(i->xml, "down_button");
    i->add_button = glade_xml_get_widget(i->xml, "add_button");
    i->remove_button = glade_xml_get_widget(i->xml, "remove_button");
    i->icon_view = glade_xml_get_widget(i->xml, "tree_view");

    glade_xml_signal_connect_data(i->xml, "on_add_button_clicked", (GCallback) on_add_button_clicked, i);
    glade_xml_signal_connect_data(i->xml, "on_remove_button_clicked", (GCallback) on_remove_button_clicked, i);
    glade_xml_signal_connect_data(i->xml, "on_up_button_clicked", (GCallback) on_up_button_clicked, i);
    glade_xml_signal_connect_data(i->xml, "on_down_button_clicked", (GCallback) on_down_button_clicked, i);

    glade_xml_signal_connect_data(i->xml, "on_close_button_clicked", (GCallback) on_close_button_clicked, i);

    g_signal_connect(G_OBJECT(i->dialog), "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    i->list_store = gtk_list_store_new(N_COLUMNS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
    gtk_icon_view_set_model(GTK_ICON_VIEW(i->icon_view), GTK_TREE_MODEL(i->list_store));
    gtk_icon_view_set_columns(GTK_ICON_VIEW(i->icon_view), G_MAXINT);

    /* FIXME: we need to have a PIXBUF column here */
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(i->icon_view), COLUMN_ICON);

    gtk_icon_view_set_text_column(GTK_ICON_VIEW(i->icon_view), COLUMN_NAME);

    gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(i->icon_view), GTK_SELECTION_SINGLE);
    g_signal_connect(i->icon_view, "selection-changed", G_CALLBACK(on_selection_changed), i);

    lassi_prefs_update(i);

    return 0;
}

void lassi_prefs_update(LassiPrefsInfo *i) {
    GList *l;
    char *selected_item = NULL;
    GtkTreeIter iter;
    GList *selected;

    g_assert(i);

    g_debug("prefs update");

    selected = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(i->icon_view));

    if (selected && gtk_tree_model_get_iter(GTK_TREE_MODEL(i->list_store), &iter, selected->data))
        gtk_tree_model_get(GTK_TREE_MODEL(i->list_store), &iter, COLUMN_NAME, &selected_item, -1);

    gtk_list_store_clear(GTK_LIST_STORE(i->list_store));

    for (l = i->server->order; l; l = l->next) {
        GError* error = NULL;
        GdkPixbuf* pixbuf;
        if (!lassi_server_is_connected(i->server, l->data))
            continue;

        pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_for_screen (gtk_widget_get_screen (i->icon_view)),
                                           strcmp(i->server->id, l->data) ? "network-wired" : "user-desktop",
                                           64, 0,
                                           &error);

        if (error) {
                g_warning ("error loading icon: %s", error->message);
                g_error_free (error);
        }

        gtk_list_store_append(GTK_LIST_STORE(i->list_store), &iter);
        gtk_list_store_set(GTK_LIST_STORE(i->list_store), &iter,
                           COLUMN_ICON, pixbuf,
                           COLUMN_NAME, l->data,
                           COLUMN_GLIST, l, -1);

        g_object_unref (pixbuf);

        if (selected_item)
            if (strcmp(selected_item, l->data) == 0) {
                GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(i->list_store), &iter);
                gtk_icon_view_select_path(GTK_ICON_VIEW(i->icon_view), path);
                gtk_tree_path_free(path);
            }
    }

    g_free(selected_item);

    update_sensitive(i);

    g_list_foreach(selected, (GFunc)gtk_tree_path_free, NULL);
    g_list_free(selected);
}

void lassi_prefs_show(LassiPrefsInfo *i) {
    g_assert(i);

    gtk_window_present(GTK_WINDOW(i->dialog));
}

void lassi_prefs_done(LassiPrefsInfo *i) {
    g_assert(i);

    g_object_unref(G_OBJECT(i->xml));
    g_object_unref(G_OBJECT(i->list_store));
    memset(i, 0, sizeof(*i));
}

