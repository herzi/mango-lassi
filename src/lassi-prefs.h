#ifndef foolassiprefshfoo
#define foolassiprefshfoo

#include <gtk/gtk.h>

typedef struct LassiPrefsInfo LassiPrefsInfo;
struct LassiServer;

struct LassiPrefsInfo {
    struct LassiServer *server;

    GtkWidget *dialog;
    GtkWidget *up_button, *down_button, *add_button, *remove_button;
    GtkWidget *icon_view;

    GtkListStore *list_store;

    gulong  row_deleted_id;
    gulong  row_inserted_id;

    GtkTreePath *inserted_path;

    GtkBuilder *builder;
};

#include "lassi-server.h"

int lassi_prefs_init(LassiPrefsInfo *i, LassiServer *server);
void lassi_prefs_show(LassiPrefsInfo *i);
void lassi_prefs_update(LassiPrefsInfo *i);
void lassi_prefs_done(LassiPrefsInfo *i);

#endif
