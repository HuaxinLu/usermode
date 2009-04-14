#!/bin/sh
set -x -e
mkdir -p admin
glib-gettextize -f
intltoolize --force

aclocal
autoconf -Wall
autoheader -Wall
automake -Wall --add-missing
#./configure --prefix=/usr --sysconfdir=/etc --enable-deprecation --with-selinux
