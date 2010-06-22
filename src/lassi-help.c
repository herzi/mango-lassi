/* This file is part of mango lassi
 *
 * Copyright (C) 2010  Sven Herzberg
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "lassi-help.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

void lassi_help_open (GdkScreen *screen, gchar const *document_name, gchar const *section_name) {
    GError    *error = NULL;
    gchar     *uri = g_strdup_printf("ghelp:%s%c%s",
                                     document_name,
                                     section_name ? '?' : '\0',
                                     section_name ? section_name : "");

    gtk_show_uri(screen, uri,  gtk_get_current_event_time(), &error);

    if(error != NULL) {
          GtkWidget *d = gtk_message_dialog_new(NULL,
                                                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                                "%s", _("Unable to open help file"));
          gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG (d),
                                                   "%s", error->message);

          gtk_dialog_run(GTK_DIALOG(d));

          g_error_free(error);
          gtk_widget_destroy(d);
    }

    g_free(uri);
}

/* vim:set et sw=2 cino=t0,f0,(0,{s,>2s,n-1s,^-1s,e2s: */
