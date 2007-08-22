#ifndef foolassiosdhfoo
#define foolassiosdhfoo

#include <gtk/gtk.h>

typedef struct LassiOsdInfo LassiOsdInfo;

struct LassiOsdInfo {
    GtkWidget *window, *label, *left_icon, *right_icon;
};

void lassi_osd_init(LassiOsdInfo *osd);
void lassi_osd_done(LassiOsdInfo *osd);

void lassi_osd_set_text(LassiOsdInfo *osd, const char *text, const char *icon_name_left, const char *icon_name_right);
void lassi_osd_hide(LassiOsdInfo *osd);


#endif
