#!/bin/bash

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

VERSION=1.9

run_versioned() {
    local P
    local V

    V=$(echo "$2" | sed -e 's,\.,,g')

    if [ -e "`which $1$V 2> /dev/null`" ] ; then
    	P="$1$V"
    else
	if [ -e "`which $1-$2 2> /dev/null`" ] ; then
	    P="$1-$2"
	else
	    P="$1"
	fi
    fi

    shift 2
    "$P" "$@"
}

set -ex

if [ "x$1" = "xam" ] ; then
    run_versioned automake "$VERSION" -a -c --foreign
    ./config.status
else
    rm -rf autom4te.cache
    rm -f config.cache

    touch config.rpath

    rm -f Makefile.am~ configure.ac~
    echo "no" | gettextize --copy --force
    test -f Makefile.am~ && mv Makefile.am~ Makefile.am
    test -f configure.ac~ && mv configure.ac~ configure.ac

    intltoolize --copy --force --automake
    gnome-doc-prepare --automake
    run_versioned aclocal "$VERSION" -I m4
    run_versioned autoconf 2.59 -Wall
    run_versioned autoheader 2.59
    run_versioned automake "$VERSION" --copy --foreign --add-missing

    if test "x$NOCONFIGURE" = "x"; then
        CFLAGS="-g -O0" ./configure --sysconfdir=/etc --localstatedir=/var "$@"
        make clean
    fi
fi
