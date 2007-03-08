#!/bin/sh
if test -x /bin/rpm ; then
	if test x${RPM_OPT_FLAGS} = x ; then
		RPM_OPT_FLAGS=`rpm --eval '%optflags'`
	fi
fi
set -x -e
CFLAGS="$DEFINES $RPM_OPT_FLAGS -g3 $CFLAGS" ; export CFLAGS
glib-gettextize -f
intltoolize --force
touch config.h.in
aclocal
automake -a
autoheader
autoconf
rm -f config.cache
./configure --prefix=/usr --sysconfdir=/etc --enable-maintainer-mode $@
#./configure --prefix=/usr --sysconfdir=/etc $@
