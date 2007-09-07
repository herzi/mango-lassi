#ifndef foolassitrayhfoo
#define foolassitrayhfoo

#include <gtk/gtk.h>
#include <libnotify/notification.h>

typedef struct LassiTrayInfo LassiTrayInfo;
struct LassiServer;

typedef enum LassiTrayNotificationIcon {
    LASSI_TRAY_NOTIFICATION_WELCOME,
    LASSI_TRAY_NOTIFICATION_LEFT,
    LASSI_TRAY_NOTIFICATION_RIGHT
} LassiTrayNotificationIcon;

struct LassiTrayInfo {
    struct LassiServer *server;

    GtkStatusIcon *status_icon;
};

#include "lassi-server.h"

int lassi_tray_init(LassiTrayInfo *i, LassiServer *server);
void lassi_tray_done(LassiTrayInfo *i);
void lassi_tray_update(LassiTrayInfo *i, int n_connected);

void lassi_tray_show_notification(LassiTrayInfo *i, char *summary, char *body, LassiTrayNotificationIcon icon);


#endif
