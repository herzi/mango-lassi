#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <gdk/gdkx.h>

#include <string.h>

#include "lassi-osd.h"

int lassi_osd_init(LassiOsdInfo *osd) {
    GtkWidget *hbox;
    GdkColor color;
    guint32 cardinal;
    GdkDisplay *display;

    g_assert(osd);

    memset(osd, 0, sizeof(*osd));

    osd->window = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_title(GTK_WINDOW(osd->window), "Mango Lassi OSD");
    gtk_window_stick(GTK_WINDOW(osd->window));
    gtk_window_set_keep_above(GTK_WINDOW(osd->window), TRUE);
    gtk_window_set_decorated(GTK_WINDOW(osd->window), FALSE);
    gtk_window_set_deletable(GTK_WINDOW(osd->window), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(osd->window), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(osd->window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(osd->window), TRUE);
    gtk_window_set_accept_focus(GTK_WINDOW(osd->window), FALSE);
    gtk_window_set_focus_on_map(GTK_WINDOW(osd->window), FALSE);
    gtk_window_set_gravity(GTK_WINDOW(osd->window), GDK_GRAVITY_SOUTH_WEST);

    osd->label = gtk_label_new("Test");
    gtk_misc_set_padding(GTK_MISC(osd->label), 16, 0);
/*     gtk_label_set_line_wrap(GTK_LABEL(osd->label), TRUE);  */
    osd->left_icon = gtk_image_new();
    osd->right_icon = gtk_image_new();

    hbox = gtk_hbox_new(0, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);

    gtk_box_pack_start(GTK_BOX(hbox), osd->left_icon, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), osd->label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(hbox), osd->right_icon, FALSE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(osd->window), hbox);

    gtk_widget_show(hbox);
    gtk_widget_show(osd->label);

    gdk_color_parse("#262624", &color);
    if (!gdk_colormap_alloc_color(gtk_widget_get_colormap(osd->window), &color, FALSE, FALSE))
        gdk_color_black(gtk_widget_get_colormap(osd->window), &color);
    gtk_widget_modify_bg(osd->window, GTK_STATE_NORMAL, &color);

    gtk_widget_realize(GTK_WIDGET(osd->window));

    cardinal = 0xbfffffff;
    display = gdk_drawable_get_display(osd->window->window);
    XChangeProperty(GDK_DISPLAY_XDISPLAY(display),
                    GDK_WINDOW_XID(osd->window->window),
                    gdk_x11_get_xatom_by_name_for_display(display, "_NET_WM_WINDOW_OPACITY"),
                    XA_CARDINAL, 32,
                    PropModeReplace,
                    (guchar *) &cardinal, 1);

    /*g_debug("WINDOW=%p", osd->window);*/

    return 0;
}

void lassi_osd_done(LassiOsdInfo *osd) {
    g_assert(osd);

    gtk_widget_destroy(osd->window);

    memset(osd, 0, sizeof(*osd));
}

void lassi_osd_set_text(LassiOsdInfo *osd, const char *text, const char *icon_name_left, const char *icon_name_right) {
    char *t;
    int w, h, max_width;

    g_assert(osd);
    g_assert(osd->window);

    /*g_debug("WINDOW=%p", osd->window);*/

    g_debug("Showing text '%s'", text);

    t = g_strdup_printf("<span size=\"large\" color=\"#F5F5F5\">%s</span>", text);
    gtk_label_set_markup(GTK_LABEL(osd->label), t);
    g_free(t);

    if (icon_name_left) {
        gtk_image_set_from_icon_name(GTK_IMAGE(osd->left_icon), icon_name_left, GTK_ICON_SIZE_DIALOG);
        gtk_widget_show(osd->left_icon);
    } else
        gtk_widget_hide(osd->left_icon);

    if (icon_name_right) {
        gtk_image_set_from_icon_name(GTK_IMAGE(osd->right_icon), icon_name_right, GTK_ICON_SIZE_DIALOG);
        gtk_widget_show(osd->right_icon);
    } else
        gtk_widget_hide(osd->right_icon);

    max_width = (gdk_screen_width()*18)/20;

    /*g_debug("WINDOW=%p", osd->window);*/

    gtk_widget_set_size_request(osd->window, -1, -1);

    gtk_window_get_size(GTK_WINDOW(osd->window), &w, &h);

    /*g_debug("WINDOW=%p", osd->window);*/

    if (w > max_width) {
        gtk_widget_set_size_request(osd->window, max_width, -1);
        w = max_width;
    }

    if (!icon_name_left == !icon_name_right) {
        gtk_label_set_justify(GTK_LABEL(osd->label), GTK_JUSTIFY_CENTER);
        gtk_window_move(GTK_WINDOW(osd->window), (gdk_screen_width() - w)/2, (gdk_screen_height()*9)/10 - h);
    } else if (icon_name_left) {
        gtk_label_set_justify(GTK_LABEL(osd->label), GTK_JUSTIFY_LEFT);
        gtk_window_move(GTK_WINDOW(osd->window), gdk_screen_width()/20, (gdk_screen_height()*9)/10 - h);
    } else {
        gtk_label_set_justify(GTK_LABEL(osd->label), GTK_JUSTIFY_RIGHT);
        gtk_window_move(GTK_WINDOW(osd->window), (gdk_screen_width()*19)/20 - w, (gdk_screen_height()*9)/10 - h);
    }

    gtk_widget_show(osd->window);

    g_debug("osd shown");
}

void lassi_osd_hide(LassiOsdInfo *osd) {
    g_assert(osd);

    gtk_widget_hide(osd->window);

    g_debug("osd hidden");
}
