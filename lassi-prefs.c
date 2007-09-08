#include <string.h>

#include "lassi-prefs.h"
#include "lassi-server.h"

static void on_add_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
}

static void on_remove_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
}

static void on_up_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
}

static void on_down_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
}

static void on_close_button_clicked(GtkButton *widget, LassiPrefsInfo *i) {
    gtk_widget_hide(GTK_WIDGET(i->dialog));
}


int lassi_prefs_init(LassiPrefsInfo *i, LassiServer *server) {
    g_assert(i);
    g_assert(server);

    memset(i, 0, sizeof(*i));
    i->server = server;

    i->xml = glade_xml_new("mango-lassi.glade", NULL, NULL);

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
    
    i->list_store = gtk_list_store_new(1, G_TYPE_STRING);

    return 0;
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
