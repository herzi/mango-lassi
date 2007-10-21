#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include <string.h>

#include "lassi-tray.h"
#include "lassi-server.h"

#define ICON_IDLE "network-wired"
#define ICON_BUSY "network-workgroup"

static void on_prefs_activate(GtkMenuItem *menuitem, LassiTrayInfo *i) {
    lassi_prefs_show(&i->server->prefs_info);
}

static void on_tray_activate(GtkStatusIcon *status_icon, LassiTrayInfo *i)  {
    gtk_menu_popup(GTK_MENU(i->menu), NULL, NULL, gtk_status_icon_position_menu, i->status_icon, 0, gtk_get_current_event_time());
}

static void on_tray_popup_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, LassiTrayInfo *i)  {
    on_tray_activate(status_icon, i);
}

int lassi_tray_init(LassiTrayInfo *i, LassiServer *server) {
    GtkWidget *item;
    g_assert(i);
    g_assert(server);

    memset(i, 0, sizeof(*i));
    i->server = server;

    notify_init("Mango Lassi");

    i->status_icon = gtk_status_icon_new_from_icon_name(ICON_IDLE);

    i->menu = gtk_menu_new();
    item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PREFERENCES, NULL);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_prefs_activate), i);
    gtk_menu_shell_append(GTK_MENU_SHELL(i->menu), item);
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(i->menu), item);
    item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, NULL);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(i->menu), item);
    gtk_widget_show_all(i->menu);

    g_signal_connect(G_OBJECT(i->status_icon), "popup_menu", G_CALLBACK(on_tray_popup_menu), i);
    g_signal_connect(G_OBJECT(i->status_icon), "activate", G_CALLBACK(on_tray_activate), i);

    lassi_tray_update(i, 0);

    return 0;
}

void lassi_tray_update(LassiTrayInfo *i, int n_connected) {
    char *t;
    g_assert(i);

    gtk_status_icon_set_from_icon_name(i->status_icon, n_connected > 0 ? ICON_BUSY : ICON_IDLE);

    if (n_connected == 0)
        t = g_strdup("No desktops connected.");
    else if (n_connected == 1)
        t = g_strdup("1 desktop connected.");
    else
        t = g_strdup_printf("%i desktops connected.", n_connected);

    gtk_status_icon_set_tooltip(i->status_icon, t);

    g_free(t);
}

void lassi_tray_show_notification(LassiTrayInfo *i, char *summary, char *body, LassiTrayNotificationIcon icon) {

    static const char * const icon_name[] = {
        [LASSI_TRAY_NOTIFICATION_WELCOME] = "user-desktop",
        [LASSI_TRAY_NOTIFICATION_LEFT] = "go-previous",
        [LASSI_TRAY_NOTIFICATION_RIGHT] = "go-next"
    };

    NotifyNotification *n;

    n = notify_notification_new_with_status_icon(summary, body, icon_name[icon], i->status_icon);
    notify_notification_set_timeout(n, 10000);
    notify_notification_set_urgency(n, NOTIFY_URGENCY_LOW);
    notify_notification_set_category(n, "network");
    notify_notification_show(n, NULL);

}

void lassi_tray_done(LassiTrayInfo *i) {
    g_assert(i);

    g_object_unref(G_OBJECT(i->status_icon));

    notify_uninit();

    memset(i, 0, sizeof(*i));
}
