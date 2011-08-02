#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libnotify/notify.h>

#include <string.h>

#include "lassi-help.h"
#include "lassi-tray.h"
#include "lassi-server.h"

#include <glib/gi18n.h>

#ifdef HAS_APP_INDICATOR
#include <libappindicator/app-indicator.h>
#endif

#define ICON_IDLE "network-wired"
#define ICON_BUSY "network-workgroup"

#if !GTK_CHECK_VERSION(2,16,0)
#define gtk_status_icon_set_tooltip_text(wid,text) gtk_status_icon_set_tooltip(wid,text)
#endif

#if GTK_CHECK_VERSION(2,14,0)
static void on_help_activate(GtkAction *action, LassiTrayInfo *i) {
#ifndef HAS_APP_INDICATOR
    lassi_help_open(gtk_status_icon_get_screen(i->status_icon), "mango-lassi", "intro");
#else
    lassi_help_open(gdk_screen_get_default(), "mango-lassi", "intro");
#endif
}
#endif

static void on_prefs_activate(GtkAction *action, LassiTrayInfo *i) {
    lassi_prefs_show(&i->server->prefs_info);
}

#ifndef HAS_APP_INDICATOR
static void on_tray_activate(GtkStatusIcon *status_icon, LassiTrayInfo *i)  {
    gtk_menu_popup(GTK_MENU(i->menu), NULL, NULL, gtk_status_icon_position_menu, i->status_icon, 0, gtk_get_current_event_time());
}
#else
static void on_tray_activate(LassiTrayInfo *i)  {
    app_indicator_set_menu (i->status_icon, GTK_MENU(i->menu));
}
#endif

#ifndef HAS_APP_INDICATOR
static void on_tray_popup_menu(GtkStatusIcon *status_icon, guint button, guint activate_time, LassiTrayInfo *i)  {
    on_tray_activate(status_icon, i);
}
#else
#endif

int lassi_tray_init(LassiTrayInfo *i, LassiServer *server) {
    GtkActionEntry  entries[] =
    {
#if GTK_CHECK_VERSION(2,14,0)
        {"Help", GTK_STOCK_HELP, NULL,
         NULL, NULL,
         G_CALLBACK (on_help_activate)},
#endif
        {"Preferences", GTK_STOCK_PREFERENCES, NULL,
         NULL, NULL,
         G_CALLBACK (on_prefs_activate)},
        {"Quit", GTK_STOCK_QUIT, NULL,
         NULL, NULL,
         G_CALLBACK (gtk_main_quit)}
    };
    GtkActionGroup *actions;
    GError         *error = NULL;

    g_assert(i);
    g_assert(server);

    memset(i, 0, sizeof(*i));
    i->server = server;

    notify_init("Mango Lassi");

#ifdef HAS_APP_INDICATOR
    i->status_icon = app_indicator_new("mango-lassi", ICON_IDLE, APP_INDICATOR_CATEGORY_SYSTEM_SERVICES);
#else
    i->status_icon = gtk_status_icon_new_from_icon_name(ICON_IDLE);
#endif

    i->ui_manager = gtk_ui_manager_new ();
    actions = gtk_action_group_new ("mango-lassi-popup");
    gtk_action_group_add_actions (actions,
                                  entries,
                                  G_N_ELEMENTS (entries),
                                  i);
    gtk_ui_manager_insert_action_group (i->ui_manager, actions, -1);
    gtk_ui_manager_add_ui_from_string (i->ui_manager,
                                       "<popup>"
                                         "<menuitem action='Preferences'/>"
#if GTK_CHECK_VERSION(2,14,0)
                                         "<menuitem action='Help'/>"
#endif
                                         "<separator />"
                                         "<menuitem action='Quit'/>"
                                       "</popup>",
                                       -1,
                                       &error);
    if (error) {
        GtkWidget* dialog = gtk_message_dialog_new (NULL, 0,
                                                    GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                                    "%s", _("Initialization Error"));
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                  _("Cannot initialize the user interface: %s"),
                                                  error->message);

        g_error_free (error);

        gtk_dialog_run (GTK_DIALOG (dialog));

        gtk_widget_destroy (dialog);
        return 1;
    }

    i->menu = gtk_ui_manager_get_widget (i->ui_manager, "/ui/popup");

#ifndef HAS_APP_INDICATOR
    g_signal_connect(G_OBJECT(i->status_icon), "popup_menu", G_CALLBACK(on_tray_popup_menu), i);
    g_signal_connect(G_OBJECT(i->status_icon), "activate", G_CALLBACK(on_tray_activate), i);
#else
    app_indicator_set_menu(i->status_icon, GTK_MENU(i->menu));
#endif

    lassi_tray_update(i, 0);

    return 0;
}

void lassi_tray_update(LassiTrayInfo *i, int n_connected) {
    char *t;
    g_assert(i);

#ifndef HAS_APP_INDICATOR
    gtk_status_icon_set_from_icon_name(i->status_icon, n_connected > 0 ? ICON_BUSY : ICON_IDLE);
#else
#endif

    if (n_connected == 0)
        t = g_strdup("No desktops connected.");
    else if (n_connected == 1)
        t = g_strdup("1 desktop connected.");
    else
        t = g_strdup_printf("%i desktops connected.", n_connected);

#ifndef HAS_APP_INDICATOR
    gtk_status_icon_set_tooltip_text(i->status_icon, t);
#endif

    g_free(t);
}

void lassi_tray_show_notification(LassiTrayInfo *i, char *summary, char *body, LassiTrayNotificationIcon icon) {

    static const char * const icon_name[] = {
        [LASSI_TRAY_NOTIFICATION_WELCOME] = "user-desktop",
        [LASSI_TRAY_NOTIFICATION_LEFT] = "go-previous",
        [LASSI_TRAY_NOTIFICATION_RIGHT] = "go-next"
    };

    NotifyNotification *n;

#ifndef HAS_APP_INDICATOR
    n = notify_notification_new_with_status_icon(summary, body, icon_name[icon], i->status_icon);
#else
    n = notify_notification_new(summary, body, icon_name[icon]);
#endif
    notify_notification_set_timeout(n, 10000);
    notify_notification_set_urgency(n, NOTIFY_URGENCY_LOW);
    notify_notification_set_category(n, "network");
    notify_notification_show(n, NULL);

}

void lassi_tray_done(LassiTrayInfo *i) {
    g_assert(i);

    if (i->status_icon) {
        g_object_unref(G_OBJECT(i->status_icon));
    }

    if (i->ui_manager) {
        g_object_unref (i->ui_manager);
    }

    notify_uninit();

    memset(i, 0, sizeof(*i));
}

/* vim:set sw=4 et: */
