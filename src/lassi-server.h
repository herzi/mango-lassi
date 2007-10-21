#ifndef foolassiserverhfoo
#define foolassiserverhfoo

#include <dbus/dbus.h>
#include <glib.h>

typedef struct LassiServer LassiServer;
typedef struct LassiConnection LassiConnection;

#include "lassi-grab.h"
#include "lassi-osd.h"
#include "lassi-clipboard.h"
#include "lassi-avahi.h"
#include "lassi-tray.h"
#include "lassi-prefs.h"

struct LassiServer {
    DBusServer *dbus_server;

    char *id, *address;
    uint16_t port;
    
    /* All connections */
    GList *connections;
    int n_connections;

    /* Configured connections */
    GHashTable *connections_by_id;
    GList *connections_left, *connections_right; /* stored from right to left, resp, left to right */

    /* Active display management */
    int active_generation;
    LassiConnection *active_connection;

    /* Layout management */
    int order_generation;
    GList *order;

    /* Clipboard CLIPBOARD management */
    int clipboard_generation;
    LassiConnection *clipboard_connection;
    gboolean clipboard_empty;

    /* Clipboard PRIMARY management */
    int primary_generation;
    LassiConnection *primary_connection;
    gboolean primary_empty;
    
    LassiGrabInfo grab_info;
    LassiOsdInfo osd_info;
    LassiClipboardInfo clipboard_info;
    LassiAvahiInfo avahi_info;
    LassiTrayInfo tray_info;
    LassiPrefsInfo prefs_info;
};

struct LassiConnection {
    LassiServer *server;
    
    DBusConnection *dbus_connection;
    char *id, *address;

    gboolean we_are_client;
    gboolean delayed_welcome;
};

int lassi_server_change_grab(LassiServer *s, gboolean to_left, int y);
int lassi_server_acquire_grab(LassiServer *s);

int lassi_server_motion_event(LassiServer *s, int dx, int dy);
int lassi_server_button_event(LassiServer *ls, unsigned button, gboolean is_press);
int lassi_server_key_event(LassiServer *ls, unsigned key, gboolean is_press);

int lassi_server_acquire_clipboard(LassiServer *ls, gboolean primary, char**targets);
int lassi_server_return_clipboard(LassiServer *ls, gboolean primary);
int lassi_server_get_clipboard(LassiServer *ls, gboolean primary, const char *t, int *f, gpointer *p, int *l);

LassiConnection* lassi_server_connect(LassiServer *ls, const char *a);
void lassi_server_disconnect(LassiServer *ls, const char *id, gboolean remove_from_order);
        
gboolean lassi_server_is_connected(LassiServer *ls, const char *id);
gboolean lassi_server_is_known(LassiServer *ls, const char *id);

#endif
