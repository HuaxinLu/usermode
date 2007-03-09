#!/bin/sh
set -x -e
mkdir -p admin
glib-gettextize -f
# intltool bug: it tries to use $aux_dir/po/Makefile.in.in
ln -s ../po admin/po
intltoolize --force
rm admin/po

aclocal
autoconf -Wall
autoheader -Wall
automake -Wall --add-missing
#./configure --prefix=/usr --sysconfdir=/etc --enable-deprecation --with-selinux
