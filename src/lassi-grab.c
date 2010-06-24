#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <X11/extensions/XKB.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "lassi-server.h"
#include "lassi-grab.h"

#define TRIGGER_WIDTH 1

static int local2global(LassiGrabInfo *i, int y) {
    g_assert(i);
    g_assert(y >= 0 && y <= gdk_screen_get_height(i->screen)-1);

    /* Convert local screen coordinates (0 .. height) into global ones (0 . 65535) */
    return (y * 0xFFFF) / (gdk_screen_get_height(i->screen)-1);
}

static int global2local(LassiGrabInfo *i, int y) {
    g_assert(i);
    g_assert(y >= 0 && y <= 0xFFFF);

    /* Convert global screen coordinates (0 . 65535) into local ones (0 .. height) */
    return (y * (gdk_screen_get_height(i->screen)-1)) / 0xFFFF;
}

static void move_pointer(LassiGrabInfo *i, int x, int y) {
    g_assert(i);

    /* Move the pointer ... */
    gdk_display_warp_pointer(i->display, i->screen, x, y);

    i->last_x = x;
    i->last_y = y;
}

static void drop_motion_events(LassiGrabInfo *i) {
    XEvent txe;

    g_assert(i);

    /* Drop all queued motion events */
    while (XCheckTypedEvent(GDK_DISPLAY_XDISPLAY(i->display), MotionNotify, &txe))
        ;
}

static int grab_input(LassiGrabInfo *i, GdkWindow *w) {
    g_assert(i);
    g_assert(w);

    if (gdk_pointer_grab(w, TRUE,
                         GDK_POINTER_MOTION_MASK|
                         GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK,
                         NULL, i->empty_cursor, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS) {
        g_debug("pointer grab failed");
        return -1;
    }


    if (gdk_keyboard_grab(w, TRUE, GDK_CURRENT_TIME) != GDK_GRAB_SUCCESS) {
        gdk_display_pointer_ungrab(i->display, GDK_CURRENT_TIME);
        g_debug("keyboard grab failed");
        return -1;
    }

    XTestGrabControl(GDK_DISPLAY_XDISPLAY(i->display), False);

    if (i->grab_window != w) {
        /* Now, rebase the pointer, so that we can easily calculate
         * relative movements */
        move_pointer(i, i->base_x, i->base_y);

        i->grab_window = w;

        i->left_shift = i->right_shift = i->double_shift = FALSE;

        g_debug("Input now grabbed");
    }

    return 0;
}

int lassi_grab_start(LassiGrabInfo *i, gboolean to_left) {
    g_assert(i);

    return grab_input(i, to_left ? i->left_window : i->right_window);
}

void lassi_grab_stop(LassiGrabInfo *i, int y) {
    int x;

    g_assert(i);

    if (!i->grab_window)
        return;

    /* Move the pointer back into our screen */
    if (y >= 0 && y < 0xFFFF) {

        /* We received a valid y coordinate, so let's use it */
        y = global2local(i, y);

        if (i->grab_window == i->left_window)
            x = TRIGGER_WIDTH;
        else
            x = gdk_screen_get_width(i->screen)-TRIGGER_WIDTH-1;

    } else {

        /* We received an invlid y coordinate, so let's center the
         * pointer */
        x = i->base_x;
        y = i->base_y;
    }

    move_pointer(i, x, y);

    gdk_display_keyboard_ungrab(i->display, GDK_CURRENT_TIME);
    gdk_display_pointer_ungrab(i->display, GDK_CURRENT_TIME);

    drop_motion_events(i);

    i->grab_window = NULL;

    g_debug("Input now ungrabbed");

    XTestGrabControl(GDK_DISPLAY_XDISPLAY(i->display), True);
}

static void handle_motion(LassiGrabInfo *i, int x, int y) {
    int dx, dy;
    int r;
    int w, h;

    dx = x - i->last_x;
    dy = y - i->last_y;

    i->last_x = x;
    i->last_y = y;

/*     g_debug("rel motion %i %i", dx, dy); */

    w = gdk_screen_get_width(i->screen);
    h = gdk_screen_get_height(i->screen);

    if (x <= w/10 || y <= h/10 ||
        x >= (w*9)/10 || y >= (h*9)/10) {

        XEvent txe;

        /* Pointer is too near to the edges, move cursor
         * back to center, so that further movements are
         * not clipped */

        g_debug("centering");

        /* First, make sure there is no further motion event in the queue */
        while (XCheckTypedEvent(GDK_DISPLAY_XDISPLAY(i->display), MotionNotify, &txe)) {
            dx += txe.xmotion.x - i->last_x;
            dy += txe.xmotion.y - i->last_y;

            i->last_x = txe.xmotion.x;
            i->last_y = txe.xmotion.y;
        }

        move_pointer(i, i->base_x, i->base_y);
    }

    /* Filter out non-existant or too large motions */
    if ((dx != 0 || dy != 0) &&
        ((abs(dx) <= (w*9)/20) && (abs(dy) <= (h*9)/20))) {

/*         g_debug("sending motion"); */

        /* Send the event */
        r = lassi_server_motion_event(i->server, dx, dy);
        g_assert(r >= 0);
    }
}

static GdkFilterReturn filter_func(GdkXEvent *gxe, GdkEvent *event, gpointer data) {
    LassiGrabInfo *i = data;
    XEvent *xe = (XEvent*) gxe;
    GdkWindow *w = ((GdkEventAny*) event)->window;

    g_assert(i);
    g_assert(xe);
    g_assert(event);

    switch (xe->type){

        case EnterNotify: {
            XEnterWindowEvent *ewe = (XEnterWindowEvent*) xe;

            if (ewe->mode == NotifyNormal && (ewe->state & i->lock_mask) == 0 && !i->grab_window) {
                g_debug("enter %u %u", ewe->x_root, ewe->y_root);

                /* Only honour this when no button/key is pressed */

                if (lassi_server_change_grab(i->server, w == i->left_window, local2global(i, ewe->y_root)) >= 0)
                    grab_input(i, w);

            } else if (i->grab_window)
                handle_motion(i, ewe->x_root, ewe->y_root);

            break;
        }

        case MotionNotify:

            if (i->grab_window) {
                XMotionEvent *me = (XMotionEvent*) xe;

/*                 g_debug("motion %u %u", me->x_root, me->y_root); */
                handle_motion(i, me->x_root, me->y_root);
            }

            break;

        case ButtonPress:
        case ButtonRelease:

            if (i->grab_window) {
                int r;
                XButtonEvent *be = (XButtonEvent*) xe;

/*                 g_debug("button press/release"); */
                handle_motion(i,  be->x_root, be->y_root);

                /* Send the event */
                r = lassi_server_button_event(i->server, be->button, xe->type == ButtonPress);
                g_assert(r >= 0);
            }
            break;

        case KeyPress:
        case KeyRelease:

/*             g_debug("raw key"); */

            if (i->grab_window) {
                int r;
                XKeyEvent *ke = (XKeyEvent *) xe;
                KeySym keysym;

                keysym = XKeycodeToKeysym(GDK_DISPLAY_XDISPLAY(i->display), ke->keycode, 0);

                if (keysym == XK_Shift_L)
                    i->left_shift = ke->type == KeyPress;
                if (keysym == XK_Shift_R)
                    i->right_shift = xe->type == KeyPress;

                if (i->left_shift && i->right_shift)
                    i->double_shift = TRUE;

/*                 g_debug("left_shift=%i right_shift=%i 0x04%x", i->left_shift, i->right_shift, (unsigned) keysym); */

/*                 g_debug("key press/release"); */
                handle_motion(i,  ke->x_root, ke->y_root);

                /* Send the event */
                r = lassi_server_key_event(i->server, keysym, xe->type == KeyPress);
                g_assert(r >= 0);

                if (!i->left_shift && !i->right_shift && i->double_shift) {
/*                     g_debug("Got double shift"); */
                    lassi_server_acquire_grab(i->server);
                    lassi_grab_stop(i, -1);
                }
            }
            break;
    }

    return GDK_FILTER_CONTINUE;
}

static unsigned int get_lock_mask(LassiGrabInfo *i, LassiServer *s) {
    XModifierKeymap *map;
    int max_ks_offset = 15;
    int mod, j;
    int k = 0;
    int maj_version = XkbMajorVersion, min_version = XkbMinorVersion;
    int xkbopcode, xkbevent, xkberror;
    int min_keycode, max_keycode, keysyms_per_keycode;
    Bool rv;
    unsigned int inv_lock_mask = 0; 

    /* ensure that xkb is present and compatible */
    rv = XkbQueryExtension(GDK_DISPLAY_XDISPLAY(i->display), 
        &xkbopcode, &xkbevent, &xkberror, &maj_version, &min_version);
    if (!rv) {
        g_debug( "XKB not supported - skipping detection of Lock Mask" );
        return ~0;
    }
    
    /* detect max number of keysyms that can be attached to a keycode */
    XDisplayKeycodes(GDK_DISPLAY_XDISPLAY(i->display), &min_keycode, &max_keycode);
    XGetKeyboardMapping(GDK_DISPLAY_XDISPLAY(i->display), min_keycode, 
        (max_keycode - min_keycode + 1), &keysyms_per_keycode);

    /* scan all modifiers and mask those that have a KeySym that _Lock's */
    map = XGetModifierMapping(GDK_DISPLAY_XDISPLAY(i->display));
    for (mod = 0; mod < 8; mod++) {
        for (j = 0; j < map->max_keypermod; j++) {
            int kc = map->modifiermap[k++];
            KeySym ks;
            int ks_offset;
            char* kn;
            size_t kn_len;
            
            if (!kc) continue;
            
            for (ks_offset = 0; ks_offset < max_ks_offset; ks_offset++) {
                ks = XKeycodeToKeysym(GDK_DISPLAY_XDISPLAY(i->display), kc, ks_offset);
                if (ks) break;
            }
            
            if (!ks) continue;
            
            kn = XKeysymToString(ks);
            kn_len = kn ? strlen(kn) : 0;
            
            /* g_debug( "\t0x%0x -> %x -> %s", kc, ks, kn ); */
            if (kn_len > 5 && 0 == strcmp(&(kn[strlen(kn)-5]),"_Lock")) {
                /* g_debug( "Lock Key Detected @ %s", &(kn[strlen(kn)-5]) ); */
                inv_lock_mask |= (1 << mod);
            } 
        }
    }
    
    /* g_debug( "Lock Mask: 0x%0x", inv_lock_mask ); */
    return ~inv_lock_mask;
}

int lassi_grab_init(LassiGrabInfo *i, LassiServer *s) {
    GdkWindowAttr wa;
    GdkColor black = { 0, 0, 0, 0 };
    const gchar cursor_data[1] = { 0 };
    GdkBitmap *bitmap;
    int xtest_event_base, xtest_error_base;
    int major_version, minor_version;

    memset(i, 0, sizeof(*i));
    i->server = s;

    i->screen = gdk_screen_get_default();
    i->display = gdk_screen_get_display(i->screen);
    i->root = gdk_screen_get_root_window(i->screen);

    if (!XTestQueryExtension(GDK_DISPLAY_XDISPLAY(i->display), &xtest_event_base, &xtest_error_base, &major_version, &minor_version)) {
        g_warning("XTest extension not supported.");
        return -1;
    }

    g_debug("XTest %u.%u supported.", major_version, minor_version);

    /* Get mask for Lock modifiers */
    i->lock_mask = get_lock_mask(i,s);

    /* Create empty cursor */
    bitmap = gdk_bitmap_create_from_data(NULL, cursor_data, 1, 1);
    i->empty_cursor = gdk_cursor_new_from_pixmap(bitmap, bitmap, &black, &black, 0, 0);
    g_object_unref(bitmap);

    /* Create trigger windows */
    memset(&wa, 0, sizeof(wa));

    wa.title = (char*) "Mango Lassi Left";
    wa.event_mask = GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_KEY_PRESS_MASK|GDK_KEY_RELEASE_MASK|GDK_ENTER_NOTIFY_MASK;
    wa.x = 0;
    wa.y = gdk_screen_get_height(i->screen)/20;
    wa.width = TRIGGER_WIDTH;
    wa.height = (gdk_screen_get_height(i->screen)*18)/20;
    wa.wclass = GDK_INPUT_ONLY;
    wa.window_type = GDK_WINDOW_FOREIGN;
    wa.override_redirect = TRUE;
    wa.type_hint = GDK_WINDOW_TYPE_HINT_DOCK;
    wa.cursor = i->empty_cursor;

    i->left_window = gdk_window_new(i->root, &wa, GDK_WA_TITLE|GDK_WA_X|GDK_WA_Y|GDK_WA_NOREDIR|GDK_WA_TYPE_HINT|GDK_WA_CURSOR);
    gdk_window_set_keep_above(i->left_window, TRUE);
    gdk_window_add_filter(i->left_window, filter_func, i);

    wa.title = (char*) "Mango Lassi Right";
    wa.x = gdk_screen_get_width(i->screen) - TRIGGER_WIDTH;

    i->right_window = gdk_window_new(i->root, &wa, GDK_WA_TITLE|GDK_WA_X|GDK_WA_Y|GDK_WA_NOREDIR|GDK_WA_TYPE_HINT);
    gdk_window_set_keep_above(i->right_window, TRUE);
    gdk_window_add_filter(i->right_window, filter_func, i);

    i->base_x = gdk_screen_get_width(i->screen)/2;
    i->base_y = gdk_screen_get_height(i->screen)/2;

    XTestGrabControl(GDK_DISPLAY_XDISPLAY(i->display), True);

    return 0;
}

void lassi_grab_done(LassiGrabInfo *i) {
    g_assert(i);

    lassi_grab_stop(i, -1);

    if (i->left_window)
        gdk_window_destroy(i->left_window);

    if (i->right_window)
        gdk_window_destroy(i->right_window);

    if (i->empty_cursor)
        gdk_cursor_unref(i->empty_cursor);
}

void lassi_grab_enable_triggers(LassiGrabInfo *i, gboolean left, gboolean right) {
    g_assert(i);

    g_debug("Showing windows: left=%s, right=%s", left ? "yes" : "no", right ? "yes" : "no");

    if (left)
        gdk_window_show(i->left_window);
    else
        gdk_window_hide(i->left_window);

    if (right)
        gdk_window_show(i->right_window);
    else
        gdk_window_hide(i->right_window);
}

int lassi_grab_move_pointer_relative(LassiGrabInfo *i, int dx, int dy) {
    g_assert(i);

    if (i->grab_window)
        return -1;

    XTestFakeRelativeMotionEvent(GDK_DISPLAY_XDISPLAY(i->display), dx, dy, 0);
    XSync(GDK_DISPLAY_XDISPLAY(i->display), False);

    return 0;
}

int lassi_grab_press_button(LassiGrabInfo *i, unsigned button, gboolean is_press) {
    g_assert(i);

    if (i->grab_window)
        return -1;

    XTestFakeButtonEvent(GDK_DISPLAY_XDISPLAY(i->display), button, is_press, 0);
    XSync(GDK_DISPLAY_XDISPLAY(i->display), False);

    return 0;
}

int lassi_grab_press_key(LassiGrabInfo *i, unsigned key, gboolean is_press) {
    g_assert(i);
    if (i->grab_window)
        return -1;

    XTestFakeKeyEvent(GDK_DISPLAY_XDISPLAY(i->display), XKeysymToKeycode(GDK_DISPLAY_XDISPLAY(i->display), key), is_press, 0);
    XSync(GDK_DISPLAY_XDISPLAY(i->display), False);

    return 0;
}
