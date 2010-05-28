# This file is part of Mango Lassi.
#
# Copyright 2007 Lennart Poettering
# Copyright 2010 Sven Herzberg
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

localedir=$(datadir)/locale
dist_man_MANS=

CLEANFILES=$(BUILT_SOURCES)

SUBDIRS = po

bin_PROGRAMS = \
	mango-lassi

nodist_mango_lassi_SOURCES=paths.h
mango_lassi_SOURCES = \
	src/lassi-server.c src/lassi-server.h \
	src/lassi-grab.c src/lassi-grab.h \
	src/lassi-osd.c src/lassi-osd.h \
	src/lassi-order.c src/lassi-order.h \
	src/lassi-clipboard.c src/lassi-clipboard.h \
	src/lassi-avahi.c src/lassi-avahi.h \
	src/lassi-tray.c src/lassi-tray.h \
	src/lassi-prefs.c src/lassi-prefs.h

BUILT_SOURCES=$(nodist_mango_lassi_SOURCES)

mango_lassi_LDADD = \
	$(AM_LDADD) \
	$(AVAHI_LIBS) \
	$(DBUS_LIBS) \
	$(GTK_LIBS) \
	$(XTEST_LIBS) \
	$(AVAHI_LIBS) \
	$(AVAHI_UI_LIBS) \
	$(LIBNOTIFY_LIBS) \
	$(NULL)

mango_lassi_CFLAGS = \
	$(AM_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GTK_CFLAGS) \
	$(XTEST_CFLAGS) \
	$(AVAHI_CFLAGS) \
	$(AVAHI_UI_CFLAGS) \
	$(LIBNOTIFY_CFLAGS) \
	$(NULL)

paths.h: Makefile
	$(AM_V_GEN) echo "#define UI_FILE \"$(pkgdatadir)/mango-lassi.ui\"" > $@; \
	echo "#define LOCALEDIR \"$(localedir)\"" >> $@

pkgdata_DATA = src/mango-lassi.ui

EXTRA_DIST = $(pkgdata_DATA)

ACLOCAL_AMFLAGS = -I m4

include data/Makefile.inc
include icons/Makefile.inc

