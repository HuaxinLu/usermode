#!/bin/sh
if test -x /bin/rpm ; then
	if test x${RPM_OPT_FLAGS} = x ; then
		RPM_OPT_FLAGS=`rpm --eval '%optflags'`
	fi
fi
set -x -e
CFLAGS="$DEFINES $RPM_OPT_FLAGS -g3 $CFLAGS" ; export CFLAGS
libtoolize --force
glib-gettextize -f -c
intltoolize --force
touch config.h.in
aclocal
automake -a
autoheader
autoconf
test -f config.cache && rm -f config.cache || true
#./configure --prefix=/usr --sysconfdir=/etc --enable-maintainer-mode $@
