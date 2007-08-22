#ifndef foolassigrabhfoo
#define foolassigrabhfoo

#include <gdk/gdk.h>

typedef struct LassiGrabInfo LassiGrabInfo;
struct LassiServer;

struct LassiGrabInfo {
    struct LassiServer *server;
    
    GdkDisplay *display;
    GdkScreen *screen;
    GdkWindow *root;

    GdkWindow *left_window, *right_window;
    GdkCursor *empty_cursor;
    GdkWindow *grab_window;

    int base_x, base_y;
    int last_x, last_y;

    gboolean left_shift, right_shift, double_shift;
};

#include "lassi-server.h"

int lassi_grab_init(LassiGrabInfo *i, LassiServer *server);
void lassi_grab_done(LassiGrabInfo *i);

int lassi_grab_start(LassiGrabInfo *i, gboolean to_left);
void lassi_grab_stop(LassiGrabInfo *i, int y);

void lassi_grab_enable_triggers(LassiGrabInfo *i, gboolean left, gboolean right);

int lassi_grab_move_pointer_relative(LassiGrabInfo *i, int dx, int dy);
int lassi_grab_press_button(LassiGrabInfo *i, unsigned button, gboolean is_press);
int lassi_grab_press_key(LassiGrabInfo *i, unsigned key, gboolean is_press);

#endif
