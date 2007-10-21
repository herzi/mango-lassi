#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <avahi-ui/avahi-ui.h>

#include "lassi-prefs.h"
#include "lassi-server.h"

#ifndef GLADE_FILE
#define GLADE_FILE "mango-lassi.glade"
#endif

enum {
    COLUMN_ICON,
    COLUMN_NAME,
    COLUMN_GLIST,
    N_COLUMNS
};


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
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    char *id;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(i->tree_view));

     if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
         return;

     gtk_tree_model_get(GTK_TREE_MODEL(i->list_store), &iter, COLUMN_NAME, &id, -1);
     if (id) {
         lassi_server_disconnect(i->server, id, TRUE);
         g_free(id);
     }
}

static void on_up_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
/*     GtkTreeSelection *selection; */
/*     GtkTreeIter iter; */
/*     char *id; */

/*    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(i->tree_view));

     if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
         return;

     gtk_tree_model_get(GTK_TREE_MODEL(i->list_store), &iter, COLUMN_NAME, &id, -1);

     if (id) {
         GList *o = lassi_list_copy(i->server->order);
         lissi_list_move_up(o, id);
         lassi_server_set_order(i->server, o);
         g_free(id);
     }*/
}

static void on_down_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
}

static void on_close_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
    gtk_widget_hide(GTK_WIDGET(i->dialog));
}

static void update_sensitive(LassiPrefsInfo *i) {
    GtkTreeIter iter;
    GtkTreePath *path;
    gboolean is_first;
    char *id;
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(i->tree_view));

    if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
        gtk_widget_set_sensitive(i->up_button, FALSE);
        gtk_widget_set_sensitive(i->down_button, FALSE);
        gtk_widget_set_sensitive(i->remove_button, FALSE);
        return;
    }

    gtk_tree_model_get(GTK_TREE_MODEL(i->list_store), &iter, COLUMN_NAME, &id, -1);
    gtk_widget_set_sensitive(i->remove_button, strcmp(id, i->server->id) != 0);
    g_free(id);

    path = gtk_tree_model_get_path(GTK_TREE_MODEL(i->list_store), &iter);

    is_first = gtk_tree_path_prev(path);
    gtk_widget_set_sensitive(i->up_button, is_first);
    if (is_first)
        gtk_tree_path_next(path);

    gtk_tree_path_next(path);
    gtk_widget_set_sensitive(i->down_button, gtk_tree_model_get_iter(GTK_TREE_MODEL(i->list_store), &iter, path));

    gtk_tree_path_free(path);
}

static void on_selection_changed(GtkTreeSelection *selection, LassiPrefsInfo *i) {
    update_sensitive(i);
}

int lassi_prefs_init(LassiPrefsInfo *i, LassiServer *server) {
    GtkTreeViewColumn *column;
    GtkTreeSelection *selection;

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
    i->tree_view = glade_xml_get_widget(i->xml, "tree_view");

    glade_xml_signal_connect_data(i->xml, "on_add_button_clicked", (GCallback) on_add_button_clicked, i);
    glade_xml_signal_connect_data(i->xml, "on_remove_button_clicked", (GCallback) on_remove_button_clicked, i);
    glade_xml_signal_connect_data(i->xml, "on_up_button_clicked", (GCallback) on_up_button_clicked, i);
    glade_xml_signal_connect_data(i->xml, "on_down_button_clicked", (GCallback) on_down_button_clicked, i);

    glade_xml_signal_connect_data(i->xml, "on_close_button_clicked", (GCallback) on_close_button_clicked, i);

    g_signal_connect(G_OBJECT(i->dialog), "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    i->list_store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    gtk_tree_view_set_model(GTK_TREE_VIEW(i->tree_view), GTK_TREE_MODEL(i->list_store));

    column = gtk_tree_view_column_new_with_attributes("Icon", gtk_cell_renderer_pixbuf_new(), "icon-name", COLUMN_ICON, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(i->tree_view), column);

    column = gtk_tree_view_column_new_with_attributes("Name", gtk_cell_renderer_text_new(), "text", COLUMN_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(i->tree_view), column);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(i->tree_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(on_selection_changed), i);

    lassi_prefs_update(i);

    return 0;
}

void lassi_prefs_update(LassiPrefsInfo *i) {
    GList *l;
    char *selected_item = NULL;
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    g_assert(i);

    g_message("prefs update");

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(i->tree_view));

    if (gtk_tree_selection_get_selected(selection, NULL, &iter))
        gtk_tree_model_get(GTK_TREE_MODEL(i->list_store), &iter, COLUMN_NAME, &selected_item, -1);

    gtk_list_store_clear(GTK_LIST_STORE(i->list_store));

    for (l = i->server->order; l; l = l->next) {

        if (!lassi_server_is_connected(i->server, l->data))
            continue;

        gtk_list_store_append(GTK_LIST_STORE(i->list_store), &iter);
        gtk_list_store_set(GTK_LIST_STORE(i->list_store), &iter,
                           COLUMN_ICON, strcmp(i->server->id, l->data) ? "network-wired" : "user-desktop",
                           COLUMN_NAME, l->data,
                           COLUMN_GLIST, l, -1);

        if (selected_item)
            if (strcmp(selected_item, l->data) == 0)
                gtk_tree_selection_select_iter(selection, &iter);
    }

    g_free(selected_item);

    update_sensitive(i);
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
