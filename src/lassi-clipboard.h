#ifndef foolassiclipboardhfoo
#define foolassiclipboardhfoo

#include <gtk/gtk.h>

typedef struct LassiClipboardInfo LassiClipboardInfo;
struct LassiServer;

struct LassiClipboardInfo {
    struct LassiServer *server;

    GtkClipboard *clipboard, *primary;
};

#include "lassi-server.h"

int lassi_clipboard_init(LassiClipboardInfo *i, LassiServer *server);
void lassi_clipboard_done(LassiClipboardInfo *i);

void lassi_clipboard_set(LassiClipboardInfo *i, gboolean primary, char *targets[]);
void lassi_clipboard_clear(LassiClipboardInfo *i, gboolean primary);
int lassi_clipboard_get(LassiClipboardInfo *i, gboolean primary, const char *target, int *format, gpointer *p, int *l);

#endif
