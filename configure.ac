# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# This file is part of Mango Lassi.
#
# Copyright 2007 Lennart Poettering
#
# Mango Lassi is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# Mango Lassi is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with Mango Lassi; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.

AC_PREREQ(2.57)

AC_INIT([mango-lassi], 001,[http://github.com/herzi/mango-lassi/issues])
AC_CONFIG_SRCDIR([src/lassi-server.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign -Wall])

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_SUBST(PACKAGE_URL, [http://github.com/herzi/mango-lassi])

if type -p stow > /dev/null && test -d /usr/local/stow ; then
   AC_MSG_NOTICE([*** Found /usr/local/stow: default install prefix set to /usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION} ***])
   ac_default_prefix="/usr/local/stow/${PACKAGE_NAME}-${PACKAGE_VERSION}"
fi

# CC

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_GCC_TRADITIONAL
AC_GNU_SOURCE

# GCC flags

test_gcc_flag() {
    AC_LANG_CONFTEST([int main(int argc, char*argv[]) {}])
    $CC -c conftest.c $CFLAGS -o conftest.o > /dev/null 2> /dev/null
    ret=$?
    rm -f conftest.o
    return $ret
}

# If using GCC specify some additional parameters
if test "x$GCC" = "xyes" ; then

    # We use gnu99 instead of c99 because many have interpreted the standard
    # in a way that int64_t isn't defined on non-64 bit platforms.
    DESIRED_FLAGS="-std=gnu99 -Wall -W -Wextra -pedantic -pipe -Wformat -Wold-style-definition -Wdeclaration-after-statement -Wfloat-equal -Wmissing-declarations -Wmissing-prototypes -Wstrict-prototypes -Wredundant-decls -Wmissing-noreturn -Wshadow -Wendif-labels -Wpointer-arith -Wcast-align -Wwrite-strings -Winline -Wno-unused-parameter -ffast-math"

    for flag in $DESIRED_FLAGS ; do
        AC_MSG_CHECKING([whether $CC accepts $flag])
        if test_gcc_flag $flag ; then
           CFLAGS="$CFLAGS $flag"
           AC_MSG_RESULT([yes])
        else
           AC_MSG_RESULT([no])
        fi
    done
fi

#### Checks for header files. ####

# ISO
AC_HEADER_STDC

#### Typdefs, structures, etc. ####

AC_C_CONST

#### Large File-Support (LFS) ####

AC_SYS_LARGEFILE

#### pkg-config ####

# Check for pkg-config manually first, as if its not installed the
# PKG_PROG_PKG_CONFIG macro won't be defined.
AC_CHECK_PROG(have_pkg_config, pkg-config, yes, no)

if test x"$have_pkg_config" = "xno"; then
    AC_MSG_ERROR(pkg-config is required to install this program)
fi

PKG_PROG_PKG_CONFIG

#### D-Bus ####

PKG_CHECK_MODULES(DBUS, [ dbus-1 >= 1.1.1 dbus-glib-1 ])
PKG_CHECK_MODULES(GTK, [ gtk+-2.0 ])
PKG_CHECK_MODULES(XTEST, [ xtst x11 ])
PKG_CHECK_MODULES(AVAHI, [ avahi-glib avahi-client ])
PKG_CHECK_MODULES(AVAHI_UI, [ avahi-ui ])
PKG_CHECK_MODULES(LIBNOTIFY, [ libnotify ])

AM_GNU_GETTEXT([external])

IT_PROG_INTLTOOL([0.35.0])
GETTEXT_PACKAGE=mango-lassi
AC_SUBST([GETTEXT_PACKAGE])
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[Gettext package])
AM_GLIB_GNU_GETTEXT

#### documentation ####

GNOME_DOC_INIT

AC_CONFIG_FILES([
Makefile
doc/Makefile
po/Makefile.in
])

AC_OUTPUT
