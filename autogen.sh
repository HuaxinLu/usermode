#!/bin/sh
set -ex

if test x$1 == x--clean; then
  git clean -fxd
  exit
fi

mkdir -p admin
glib-gettextize -f
intltoolize --force

aclocal -Wall
autoconf -Wall
autoheader -Wall
automake -Wall --add-missing

if test x$1 == x--archive; then
  ./configure
  make dist
fi
