set -e
libtoolize -f
gettextize -f
aclocal
autoheader
automake -a
autoconf
CFLAGS=${CFLAGS:--Wall -Wunused -Wmissing-prototypes -Wmissing-declarations -g3}; export CFLAGS
rm -f config.cache
./configure --enable-debug --sysconfdir=/etc $@
