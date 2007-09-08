#ifndef foolassiprefshfoo
#define foolassiprefshfoo

#include <gtk/gtk.h>
#include <glade/glade-xml.h>

typedef struct LassiPrefsInfo LassiPrefsInfo;
struct LassiServer;

struct LassiPrefsInfo {
    struct LassiServer *server;

    GtkWidget *dialog;
    GtkWidget *up_button, *down_button, *add_button, *remove_button;
    GtkWidget *tree_view;
    
    GtkListStore *list_store;

    GladeXML *xml;
};

#include "lassi-server.h"

int lassi_prefs_init(LassiPrefsInfo *i, LassiServer *server);
void lassi_prefs_show(LassiPrefsInfo *i);
void lassi_prefs_done(LassiPrefsInfo *i);

#endif
