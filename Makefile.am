# This file is part of Mango Lassi.
#
# Copyright 207 Lennart Poettering
#
# Mango Lassi is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Mango Lassi is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with Mango Lassi; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

gladedir = $(pkgdatadir)
localedir = $(datadir)/locale

SUBDIRS = po

bin_PROGRAMS = \
	mango-lassi

mango_lassi_SOURCES = \
	src/lassi-server.c src/lassi-server.h \
	src/lassi-grab.c src/lassi-grab.h \
	src/lassi-osd.c src/lassi-osd.h \
	src/lassi-order.c src/lassi-order.h \
	src/lassi-clipboard.c src/lassi-clipboard.h \
	src/lassi-avahi.c src/lassi-avahi.h \
	src/lassi-tray.c src/lassi-tray.h \
	src/lassi-prefs.c src/lassi-prefs.h

mango_lassi_LDADD = \
	$(AM_LDADD) \
	$(AVAHI_LIBS) \
	$(DBUS_LIBS) \
	$(GTK_LIBS) \
	$(XTEST_LIBS) \
	$(AVAHI_LIBS) \
	$(AVAHI_UI_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(LIBGLADE_LIBS)

mango_lassi_CFLAGS = \
	$(AM_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GTK_CFLAGS) \
	$(XTEST_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(AVAHI_UI_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(LIBGLADE_CFLAGS) \
	-DGLADE_FILE=\"$(gladedir)/mango-lassi.glade\" \
	-DLOCALEDIR=\"$(localedir)\"

glade_DATA = \
	src/mango-lassi.glade

EXTRA_DIST = \
	intltool-merge.in \
	intltool-update.in \
	intltool-extract.in \
	src/mango-lassi.glade

DISTCLEANFILES = \
        intltool-extract \
        intltool-merge \
        intltool-update

ACLOCAL_AMFLAGS = -I m4
