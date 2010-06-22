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

#ifndef LASSI_HELP_H
#define LASSI_HELP_H

#include <gdk/gdk.h>

G_BEGIN_DECLS

void lassi_help_open (GdkScreen   *screen,
                      gchar const *document_name,
                      gchar const *section_name);

G_END_DECLS

#endif /* !LASSI_HELP_H */

/* vim:set et sw=2 cino=t0,f0,(0,{s,>2s,n-1s,^-1s,e2s: */
