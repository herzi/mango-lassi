#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include <string.h>

#include "lassi-tray.h"
#include "lassi-server.h"

#define ICON_IDLE "network-wired"
#define ICON_BUSY "network-workgroup"

int lassi_tray_init(LassiTrayInfo *i, LassiServer *server) {
    g_assert(i);
    g_assert(server);

    memset(i, 0, sizeof(*i));
    i->server = server;

    notify_init("Mango Lassi");
    
    i->status_icon = gtk_status_icon_new_from_icon_name(ICON_IDLE);

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
