# Makefile.in generated automatically by automake 1.5 from Makefile.am.

# Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
# Free Software Foundation, Inc.
# This Makefile.in is free software; the Free Software Foundation
# gives unlimited permission to copy and/or distribute it,
# with or without modifications, as long as this notice is preserved.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY, to the extent permitted by law; without
# even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.



SHELL = /bin/sh

srcdir = .
top_srcdir = .

prefix = /usr
exec_prefix = /usr

bindir = ${exec_prefix}/bin
sbindir = ${exec_prefix}/sbin
libexecdir = ${exec_prefix}/libexec
datadir = ${prefix}/share
sysconfdir = /etc
sharedstatedir = ${prefix}/com
localstatedir = ${prefix}/var
libdir = ${exec_prefix}/lib
infodir = ${prefix}/info
mandir = ${prefix}/man
includedir = ${prefix}/include
oldincludedir = /usr/include
pkgdatadir = $(datadir)/usermode
pkglibdir = $(libdir)/usermode
pkgincludedir = $(includedir)/usermode
top_builddir = .

ACLOCAL = ${SHELL} /home/devel/nalin/projects/usermode/missing --run aclocal
AUTOCONF = ${SHELL} /home/devel/nalin/projects/usermode/missing --run autoconf
AUTOMAKE = ${SHELL} /home/devel/nalin/projects/usermode/missing --run automake
AUTOHEADER = ${SHELL} /home/devel/nalin/projects/usermode/missing --run autoheader

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644
INSTALL_SCRIPT = ${INSTALL}
INSTALL_HEADER = $(INSTALL_DATA)
transform = s,x,x,
NORMAL_INSTALL = :
PRE_INSTALL = :
POST_INSTALL = :
NORMAL_UNINSTALL = :
PRE_UNINSTALL = :
POST_UNINSTALL = :
host_alias = 
host_triplet = i686-pc-linux-gnu
AMTAR = ${SHELL} /home/devel/nalin/projects/usermode/missing --run tar
AWK = gawk
BUILD_INCLUDED_LIBINTL = no
CATALOGS =  bs.gmo cs.gmo da.gmo de.gmo es.gmo eu_ES.gmo fi.gmo fr.gmo gl.gmo hu.gmo id.gmo is.gmo it.gmo ja.gmo ko.gmo no.gmo pl.gmo pt_BR.gmo pt.gmo ro.gmo ru.gmo sk.gmo sl.gmo sr.gmo sv.gmo tr.gmo uk.gmo wa.gmo zh_CN.GB2312.gmo zh.gmo
CATOBJEXT = .gmo
CC = gcc
DATADIRNAME = share
DEPDIR = .deps
EXEEXT = 
FDFORMAT = /usr/bin/fdformat
GENCAT = gencat
GLIBC21 = yes
GMOFILES =  bs.gmo cs.gmo da.gmo de.gmo es.gmo eu_ES.gmo fi.gmo fr.gmo gl.gmo hu.gmo id.gmo is.gmo it.gmo ja.gmo ko.gmo no.gmo pl.gmo pt_BR.gmo pt.gmo ro.gmo ru.gmo sk.gmo sl.gmo sr.gmo sv.gmo tr.gmo uk.gmo wa.gmo zh_CN.GB2312.gmo zh.gmo
GMSGFMT = /usr/bin/msgfmt
INSTALL_STRIP_PROGRAM = ${SHELL} $(install_sh) -c -s
INSTOBJEXT = .mo
INTLBISON = bison
INTLLIBS = 
INTLOBJS = 
INTL_LIBTOOL_SUFFIX_PREFIX = 
LIBICONV = 
MKFS = /sbin/mkfs
MKINSTALLDIRS = ./mkinstalldirs
MOUNT = /bin/mount
MSGFMT = /usr/bin/msgfmt
OBJEXT = o
PACKAGE = usermode
PAM_LIBS = -lpwdb -lpam_misc -lpam -ldl
PKG_CONFIG = /usr/bin/pkg-config
POFILES =  bs.po cs.po da.po de.po es.po eu_ES.po fi.po fr.po gl.po hu.po id.po is.po it.po ja.po ko.po no.po pl.po pt_BR.po pt.po ro.po ru.po sk.po sl.po sr.po sv.po tr.po uk.po wa.po zh_CN.GB2312.po zh.po
POSUB = po
RANLIB = ranlib
UMOUNT = /bin/umount
USE_INCLUDED_LIBINTL = no
USE_NLS = yes
am__include = include
am__quote = 
install_sh = /home/devel/nalin/projects/usermode/install-sh

VERSION = $(shell awk '/^Version:/ { print $$2 }' < usermode.spec)
RELEASE = $(shell awk '/^Release:/ { print $$2 }' < usermode.spec)
CVSTAG = usermode-$(subst .,-,$(VERSION)-$(RELEASE))
EXTRA_DIST = usermode.spec dummy.h shutdown

SUBDIRS = intl po

CFLAGS =  -I/usr/include/libglade-2.0 -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/pango-1.0 -I/usr/X11R6/include -I/usr/include/freetype2 -I/usr/include/atk-1.0 -I/usr/include/libxml2   -Wall -Wunused -Wmissing-prototypes -Wmissing-declarations -g3
consolehelper_CFLAGS = -DDISABLE_X11 $(CFLAGS)

bin_PROGRAMS = userinfo usermount userpasswd consolehelper consolehelper-x11
bin_SCRIPTS = shutdown
sbin_PROGRAMS = userhelper
noinst_PROGRAMS = test-userdialog
man1_MANS = userinfo.1 usermount.1 userpasswd.1
man8_MANS = consolehelper.8 userhelper.8

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = keys.xpm

applnkdir = $(sysconfdir)/X11/applnk/System
applnk_DATA = userinfo.desktop usermount.desktop userpasswd.desktop

consoleappdir = $(sysconfdir)/security/console.apps
WRAPPED_APPS = halt poweroff reboot
consoleapp_DATA = $(WRAPPED_APPS)

pkgdata_DATA = usermode.glade

userhelper_SOURCES = userhelper.c userhelper.h shvar.c shvar.h
userhelper_LDADD =  -L/usr/lib -lglib-1.3   -lpwdb -lpam_misc -lpam -ldl 

userinfo_SOURCES = userinfo.c userdialogs.c userhelper-wrap.c userhelper-wrap.h
userinfo_LDADD =  -L/usr/lib -L/usr/X11R6/lib -lglade-2.0 -lgtk-x11-1.3 -lgdk-x11-1.3 -lXi -lgdk_pixbuf-1.3 -lm -lpangox -lpangoxft -lXft -lXrender -lXext -lX11 -lfreetype -lpango -latk -lgobject-1.3 -lgmodule-1.3 -ldl -lglib-1.3 -lxml2 -lz   

usermount_SOURCES = usermount.c userdialogs.c
usermount_LDADD =  -L/usr/lib -L/usr/X11R6/lib -lglade-2.0 -lgtk-x11-1.3 -lgdk-x11-1.3 -lXi -lgdk_pixbuf-1.3 -lm -lpangox -lpangoxft -lXft -lXrender -lXext -lX11 -lfreetype -lpango -latk -lgobject-1.3 -lgmodule-1.3 -ldl -lglib-1.3 -lxml2 -lz   

userpasswd_SOURCES = userpasswd.c userdialogs.c userdialogs.h userhelper-wrap.c
userpasswd_LDADD =  -L/usr/lib -L/usr/X11R6/lib -lglade-2.0 -lgtk-x11-1.3 -lgdk-x11-1.3 -lXi -lgdk_pixbuf-1.3 -lm -lpangox -lpangoxft -lXft -lXrender -lXext -lX11 -lfreetype -lpango -latk -lgobject-1.3 -lgmodule-1.3 -ldl -lglib-1.3 -lxml2 -lz   

consolehelper_SOURCES = consolehelper.c
consolehelper_LDADD =  -L/usr/lib -lglib-1.3   

consolehelper_x11_SOURCES = consolehelper.c userdialogs.c userhelper-wrap.c
consolehelper_x11_LDADD =  -L/usr/lib -L/usr/X11R6/lib -lglade-2.0 -lgtk-x11-1.3 -lgdk-x11-1.3 -lXi -lgdk_pixbuf-1.3 -lm -lpangox -lpangoxft -lXft -lXrender -lXext -lX11 -lfreetype -lpango -latk -lgobject-1.3 -lgmodule-1.3 -ldl -lglib-1.3 -lxml2 -lz   

test_userdialog_SOURCES = test-userdialog.c userdialogs.c
test_userdialog_LDADD =  -L/usr/lib -L/usr/X11R6/lib -lglade-2.0 -lgtk-x11-1.3 -lgdk-x11-1.3 -lXi -lgdk_pixbuf-1.3 -lm -lpangox -lpangoxft -lXft -lXrender -lXext -lX11 -lfreetype -lpango -latk -lgobject-1.3 -lgmodule-1.3 -ldl -lglib-1.3 -lxml2 -lz   
subdir = .
ACLOCAL_M4 = $(top_srcdir)/aclocal.m4
mkinstalldirs = $(SHELL) $(top_srcdir)/mkinstalldirs
CONFIG_HEADER = config.h
CONFIG_CLEAN_FILES = intl/Makefile consolehelper.8 userhelper.8
bin_PROGRAMS = userinfo$(EXEEXT) usermount$(EXEEXT) userpasswd$(EXEEXT) \
	consolehelper$(EXEEXT) consolehelper-x11$(EXEEXT)
noinst_PROGRAMS = test-userdialog$(EXEEXT)
sbin_PROGRAMS = userhelper$(EXEEXT)
PROGRAMS = $(bin_PROGRAMS) $(noinst_PROGRAMS) $(sbin_PROGRAMS)

am_consolehelper_OBJECTS = consolehelper-consolehelper.$(OBJEXT)
consolehelper_OBJECTS = $(am_consolehelper_OBJECTS)
consolehelper_DEPENDENCIES =
consolehelper_LDFLAGS =
am_consolehelper_x11_OBJECTS = consolehelper.$(OBJEXT) \
	userdialogs.$(OBJEXT) userhelper-wrap.$(OBJEXT)
consolehelper_x11_OBJECTS = $(am_consolehelper_x11_OBJECTS)
consolehelper_x11_DEPENDENCIES =
consolehelper_x11_LDFLAGS =
am_test_userdialog_OBJECTS = test-userdialog.$(OBJEXT) \
	userdialogs.$(OBJEXT)
test_userdialog_OBJECTS = $(am_test_userdialog_OBJECTS)
test_userdialog_DEPENDENCIES =
test_userdialog_LDFLAGS =
am_userhelper_OBJECTS = userhelper.$(OBJEXT) shvar.$(OBJEXT)
userhelper_OBJECTS = $(am_userhelper_OBJECTS)
userhelper_DEPENDENCIES =
userhelper_LDFLAGS =
am_userinfo_OBJECTS = userinfo.$(OBJEXT) userdialogs.$(OBJEXT) \
	userhelper-wrap.$(OBJEXT)
userinfo_OBJECTS = $(am_userinfo_OBJECTS)
userinfo_DEPENDENCIES =
userinfo_LDFLAGS =
am_usermount_OBJECTS = usermount.$(OBJEXT) userdialogs.$(OBJEXT)
usermount_OBJECTS = $(am_usermount_OBJECTS)
usermount_DEPENDENCIES =
usermount_LDFLAGS =
am_userpasswd_OBJECTS = userpasswd.$(OBJEXT) userdialogs.$(OBJEXT) \
	userhelper-wrap.$(OBJEXT)
userpasswd_OBJECTS = $(am_userpasswd_OBJECTS)
userpasswd_DEPENDENCIES =
userpasswd_LDFLAGS =
SCRIPTS = $(bin_SCRIPTS)


DEFS = -DHAVE_CONFIG_H
DEFAULT_INCLUDES =  -I. -I$(srcdir) -I.
CPPFLAGS = 
LDFLAGS = 
LIBS = 
depcomp = $(SHELL) $(top_srcdir)/depcomp
DEP_FILES = $(DEPDIR)/consolehelper-consolehelper.Po \
	$(DEPDIR)/consolehelper.Po $(DEPDIR)/shvar.Po \
	$(DEPDIR)/test-userdialog.Po \
	$(DEPDIR)/userdialogs.Po \
	$(DEPDIR)/userhelper-wrap.Po \
	$(DEPDIR)/userhelper.Po $(DEPDIR)/userinfo.Po \
	$(DEPDIR)/usermount.Po $(DEPDIR)/userpasswd.Po
COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
CCLD = $(CC)
LINK = $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(AM_LDFLAGS) $(LDFLAGS) -o $@
DIST_SOURCES = $(consolehelper_SOURCES) $(consolehelper_x11_SOURCES) \
	$(test_userdialog_SOURCES) $(userhelper_SOURCES) \
	$(userinfo_SOURCES) $(usermount_SOURCES) $(userpasswd_SOURCES)

NROFF = nroff
MANS = $(man1_MANS) $(man8_MANS)
DATA = $(applnk_DATA) $(consoleapp_DATA) $(pixmap_DATA) $(pkgdata_DATA)


RECURSIVE_TARGETS = info-recursive dvi-recursive install-info-recursive \
	uninstall-info-recursive all-recursive install-data-recursive \
	install-exec-recursive installdirs-recursive install-recursive \
	uninstall-recursive check-recursive installcheck-recursive
DIST_COMMON = README ./stamp-h.in ABOUT-NLS AUTHORS COPYING ChangeLog \
	INSTALL Makefile.am Makefile.in NEWS aclocal.m4 compile \
	config.guess config.h.in config.sub configure configure.in \
	consolehelper.8.in depcomp install-sh ltmain.sh missing \
	mkinstalldirs userhelper.8.in
DIST_SUBDIRS = $(SUBDIRS)
SOURCES = $(consolehelper_SOURCES) $(consolehelper_x11_SOURCES) $(test_userdialog_SOURCES) $(userhelper_SOURCES) $(userinfo_SOURCES) $(usermount_SOURCES) $(userpasswd_SOURCES)

all: config.h
	$(MAKE) $(AM_MAKEFLAGS) all-recursive

.SUFFIXES:
.SUFFIXES: .c .o .obj
$(srcdir)/Makefile.in:  Makefile.am  $(top_srcdir)/configure.in $(ACLOCAL_M4)
	cd $(top_srcdir) && \
	  $(AUTOMAKE) --gnu  Makefile
Makefile:  $(srcdir)/Makefile.in  $(top_builddir)/config.status
	cd $(top_builddir) && \
	  CONFIG_HEADERS= CONFIG_LINKS= \
	  CONFIG_FILES=$@ $(SHELL) ./config.status

$(top_builddir)/config.status: $(srcdir)/configure $(CONFIG_STATUS_DEPENDENCIES)
	$(SHELL) ./config.status --recheck
$(srcdir)/configure:  $(srcdir)/configure.in $(ACLOCAL_M4) $(CONFIGURE_DEPENDENCIES)
	cd $(srcdir) && $(AUTOCONF)

$(ACLOCAL_M4):  configure.in 
	cd $(srcdir) && $(ACLOCAL) $(ACLOCAL_AMFLAGS)
config.h: stamp-h
	@if test ! -f $@; then \
		rm -f stamp-h; \
		$(MAKE) stamp-h; \
	else :; fi
stamp-h: $(srcdir)/config.h.in $(top_builddir)/config.status
	@rm -f stamp-h stamp-hT
	@echo timestamp > stamp-hT 2> /dev/null
	cd $(top_builddir) \
	  && CONFIG_FILES= CONFIG_HEADERS=config.h \
	     $(SHELL) ./config.status
	@mv stamp-hT stamp-h
$(srcdir)/config.h.in:  $(srcdir)/./stamp-h.in
	@if test ! -f $@; then \
		rm -f $(srcdir)/./stamp-h.in; \
		$(MAKE) $(srcdir)/./stamp-h.in; \
	else :; fi
$(srcdir)/./stamp-h.in: $(top_srcdir)/configure.in $(ACLOCAL_M4) 
	@rm -f $(srcdir)/./stamp-h.in $(srcdir)/./stamp-h.inT
	@echo timestamp > $(srcdir)/./stamp-h.inT 2> /dev/null
	cd $(top_srcdir) && $(AUTOHEADER)
	@mv $(srcdir)/./stamp-h.inT $(srcdir)/./stamp-h.in

distclean-hdr:
	-rm -f config.h
intl/Makefile: $(top_builddir)/config.status $(top_srcdir)/intl/Makefile.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= CONFIG_LINKS= $(SHELL) ./config.status
consolehelper.8: $(top_builddir)/config.status consolehelper.8.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= CONFIG_LINKS= $(SHELL) ./config.status
userhelper.8: $(top_builddir)/config.status userhelper.8.in
	cd $(top_builddir) && CONFIG_FILES=$@ CONFIG_HEADERS= CONFIG_LINKS= $(SHELL) ./config.status
install-binPROGRAMS: $(bin_PROGRAMS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	@list='$(bin_PROGRAMS)'; for p in $$list; do \
	  p1=`echo $$p|sed 's/$(EXEEXT)$$//'`; \
	  if test -f $$p \
	  ; then \
	    f=`echo $$p1|sed '$(transform);s/$$/$(EXEEXT)/'`; \
	   echo " $(INSTALL_PROGRAM_ENV) $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/$$f"; \
	   $(INSTALL_PROGRAM_ENV) $(INSTALL_PROGRAM) $$p $(DESTDIR)$(bindir)/$$f; \
	  else :; fi; \
	done

uninstall-binPROGRAMS:
	@$(NORMAL_UNINSTALL)
	@list='$(bin_PROGRAMS)'; for p in $$list; do \
	  f=`echo $$p|sed 's/$(EXEEXT)$$//;$(transform);s/$$/$(EXEEXT)/'`; \
	  echo " rm -f $(DESTDIR)$(bindir)/$$f"; \
	  rm -f $(DESTDIR)$(bindir)/$$f; \
	done

clean-binPROGRAMS:
	-test -z "$(bin_PROGRAMS)" || rm -f $(bin_PROGRAMS)

clean-noinstPROGRAMS:
	-test -z "$(noinst_PROGRAMS)" || rm -f $(noinst_PROGRAMS)
install-sbinPROGRAMS: $(sbin_PROGRAMS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(sbindir)
	@list='$(sbin_PROGRAMS)'; for p in $$list; do \
	  p1=`echo $$p|sed 's/$(EXEEXT)$$//'`; \
	  if test -f $$p \
	  ; then \
	    f=`echo $$p1|sed '$(transform);s/$$/$(EXEEXT)/'`; \
	   echo " $(INSTALL_PROGRAM_ENV) $(INSTALL_PROGRAM) $$p $(DESTDIR)$(sbindir)/$$f"; \
	   $(INSTALL_PROGRAM_ENV) $(INSTALL_PROGRAM) $$p $(DESTDIR)$(sbindir)/$$f; \
	  else :; fi; \
	done

uninstall-sbinPROGRAMS:
	@$(NORMAL_UNINSTALL)
	@list='$(sbin_PROGRAMS)'; for p in $$list; do \
	  f=`echo $$p|sed 's/$(EXEEXT)$$//;$(transform);s/$$/$(EXEEXT)/'`; \
	  echo " rm -f $(DESTDIR)$(sbindir)/$$f"; \
	  rm -f $(DESTDIR)$(sbindir)/$$f; \
	done

clean-sbinPROGRAMS:
	-test -z "$(sbin_PROGRAMS)" || rm -f $(sbin_PROGRAMS)
consolehelper-consolehelper.$(OBJEXT): consolehelper.c
consolehelper$(EXEEXT): $(consolehelper_OBJECTS) $(consolehelper_DEPENDENCIES) 
	@rm -f consolehelper$(EXEEXT)
	$(LINK) $(consolehelper_LDFLAGS) $(consolehelper_OBJECTS) $(consolehelper_LDADD) $(LIBS)
consolehelper-x11$(EXEEXT): $(consolehelper_x11_OBJECTS) $(consolehelper_x11_DEPENDENCIES) 
	@rm -f consolehelper-x11$(EXEEXT)
	$(LINK) $(consolehelper_x11_LDFLAGS) $(consolehelper_x11_OBJECTS) $(consolehelper_x11_LDADD) $(LIBS)
test-userdialog$(EXEEXT): $(test_userdialog_OBJECTS) $(test_userdialog_DEPENDENCIES) 
	@rm -f test-userdialog$(EXEEXT)
	$(LINK) $(test_userdialog_LDFLAGS) $(test_userdialog_OBJECTS) $(test_userdialog_LDADD) $(LIBS)
userhelper$(EXEEXT): $(userhelper_OBJECTS) $(userhelper_DEPENDENCIES) 
	@rm -f userhelper$(EXEEXT)
	$(LINK) $(userhelper_LDFLAGS) $(userhelper_OBJECTS) $(userhelper_LDADD) $(LIBS)
userinfo$(EXEEXT): $(userinfo_OBJECTS) $(userinfo_DEPENDENCIES) 
	@rm -f userinfo$(EXEEXT)
	$(LINK) $(userinfo_LDFLAGS) $(userinfo_OBJECTS) $(userinfo_LDADD) $(LIBS)
usermount$(EXEEXT): $(usermount_OBJECTS) $(usermount_DEPENDENCIES) 
	@rm -f usermount$(EXEEXT)
	$(LINK) $(usermount_LDFLAGS) $(usermount_OBJECTS) $(usermount_LDADD) $(LIBS)
userpasswd$(EXEEXT): $(userpasswd_OBJECTS) $(userpasswd_DEPENDENCIES) 
	@rm -f userpasswd$(EXEEXT)
	$(LINK) $(userpasswd_LDFLAGS) $(userpasswd_OBJECTS) $(userpasswd_LDADD) $(LIBS)
install-binSCRIPTS: $(bin_SCRIPTS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	@list='$(bin_SCRIPTS)'; for p in $$list; do \
	  f="`echo $$p|sed '$(transform)'`"; \
	  if test -f $$p; then \
	    echo " $(INSTALL_SCRIPT) $$p $(DESTDIR)$(bindir)/$$f"; \
	    $(INSTALL_SCRIPT) $$p $(DESTDIR)$(bindir)/$$f; \
	  elif test -f $(srcdir)/$$p; then \
	    echo " $(INSTALL_SCRIPT) $(srcdir)/$$p $(DESTDIR)$(bindir)/$$f"; \
	    $(INSTALL_SCRIPT) $(srcdir)/$$p $(DESTDIR)$(bindir)/$$f; \
	  else :; fi; \
	done

uninstall-binSCRIPTS:
	@$(NORMAL_UNINSTALL)
	@list='$(bin_SCRIPTS)'; for p in $$list; do \
	  f="`echo $$p|sed '$(transform)'`"; \
	  echo " rm -f $(DESTDIR)$(bindir)/$$f"; \
	  rm -f $(DESTDIR)$(bindir)/$$f; \
	done

mostlyclean-compile:
	-rm -f *.$(OBJEXT) core *.core

distclean-compile:
	-rm -f *.tab.c

include $(DEPDIR)/consolehelper-consolehelper.Po
include $(DEPDIR)/consolehelper.Po
include $(DEPDIR)/shvar.Po
include $(DEPDIR)/test-userdialog.Po
include $(DEPDIR)/userdialogs.Po
include $(DEPDIR)/userhelper-wrap.Po
include $(DEPDIR)/userhelper.Po
include $(DEPDIR)/userinfo.Po
include $(DEPDIR)/usermount.Po
include $(DEPDIR)/userpasswd.Po

distclean-depend:
	-rm -rf $(DEPDIR)

.c.o:
	source='$<' object='$@' libtool=no \
	depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' \
	$(CCDEPMODE) $(depcomp) \
	$(COMPILE) -c `test -f $< || echo '$(srcdir)/'`$<

.c.obj:
	source='$<' object='$@' libtool=no \
	depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' \
	$(CCDEPMODE) $(depcomp) \
	$(COMPILE) -c `cygpath -w $<`

consolehelper-consolehelper.o: consolehelper.c
	source='consolehelper.c' object='consolehelper-consolehelper.o' libtool=no \
	depfile='$(DEPDIR)/consolehelper-consolehelper.Po' tmpdepfile='$(DEPDIR)/consolehelper-consolehelper.TPo' \
	$(CCDEPMODE) $(depcomp) \
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(consolehelper_CFLAGS) $(CFLAGS) -c -o consolehelper-consolehelper.o `test -f consolehelper.c || echo '$(srcdir)/'`consolehelper.c

consolehelper-consolehelper.obj: consolehelper.c
	source='consolehelper.c' object='consolehelper-consolehelper.obj' libtool=no \
	depfile='$(DEPDIR)/consolehelper-consolehelper.Po' tmpdepfile='$(DEPDIR)/consolehelper-consolehelper.TPo' \
	$(CCDEPMODE) $(depcomp) \
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(consolehelper_CFLAGS) $(CFLAGS) -c -o consolehelper-consolehelper.obj `cygpath -w consolehelper.c`
CCDEPMODE = depmode=gcc3
uninstall-info-am:

man1dir = $(mandir)/man1
install-man1: $(man1_MANS) $(man_MANS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(man1dir)
	@list='$(man1_MANS) $(dist_man1_MANS) $(nodist_man1_MANS)'; \
	l2='$(man_MANS) $(dist_man_MANS) $(nodist_man_MANS)'; \
	for i in $$l2; do \
	  case "$$i" in \
	    *.1*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  if test -f $(srcdir)/$$i; then file=$(srcdir)/$$i; \
	  else file=$$i; fi; \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " $(INSTALL_DATA) $$file $(DESTDIR)$(man1dir)/$$inst"; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(man1dir)/$$inst; \
	done
uninstall-man1:
	@$(NORMAL_UNINSTALL)
	@list='$(man1_MANS) $(dist_man1_MANS) $(nodist_man1_MANS)'; \
	l2='$(man_MANS) $(dist_man_MANS) $(nodist_man_MANS)'; \
	for i in $$l2; do \
	  case "$$i" in \
	    *.1*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " rm -f $(DESTDIR)$(man1dir)/$$inst"; \
	  rm -f $(DESTDIR)$(man1dir)/$$inst; \
	done

man8dir = $(mandir)/man8
install-man8: $(man8_MANS) $(man_MANS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(man8dir)
	@list='$(man8_MANS) $(dist_man8_MANS) $(nodist_man8_MANS)'; \
	l2='$(man_MANS) $(dist_man_MANS) $(nodist_man_MANS)'; \
	for i in $$l2; do \
	  case "$$i" in \
	    *.8*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  if test -f $(srcdir)/$$i; then file=$(srcdir)/$$i; \
	  else file=$$i; fi; \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " $(INSTALL_DATA) $$file $(DESTDIR)$(man8dir)/$$inst"; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(man8dir)/$$inst; \
	done
uninstall-man8:
	@$(NORMAL_UNINSTALL)
	@list='$(man8_MANS) $(dist_man8_MANS) $(nodist_man8_MANS)'; \
	l2='$(man_MANS) $(dist_man_MANS) $(nodist_man_MANS)'; \
	for i in $$l2; do \
	  case "$$i" in \
	    *.8*) list="$$list $$i" ;; \
	  esac; \
	done; \
	for i in $$list; do \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " rm -f $(DESTDIR)$(man8dir)/$$inst"; \
	  rm -f $(DESTDIR)$(man8dir)/$$inst; \
	done
install-applnkDATA: $(applnk_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(applnkdir)
	@list='$(applnk_DATA)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " $(INSTALL_DATA) $$d$$p $(DESTDIR)$(applnkdir)/$$f"; \
	  $(INSTALL_DATA) $$d$$p $(DESTDIR)$(applnkdir)/$$f; \
	done

uninstall-applnkDATA:
	@$(NORMAL_UNINSTALL)
	@list='$(applnk_DATA)'; for p in $$list; do \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " rm -f $(DESTDIR)$(applnkdir)/$$f"; \
	  rm -f $(DESTDIR)$(applnkdir)/$$f; \
	done
install-consoleappDATA: $(consoleapp_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(consoleappdir)
	@list='$(consoleapp_DATA)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " $(INSTALL_DATA) $$d$$p $(DESTDIR)$(consoleappdir)/$$f"; \
	  $(INSTALL_DATA) $$d$$p $(DESTDIR)$(consoleappdir)/$$f; \
	done

uninstall-consoleappDATA:
	@$(NORMAL_UNINSTALL)
	@list='$(consoleapp_DATA)'; for p in $$list; do \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " rm -f $(DESTDIR)$(consoleappdir)/$$f"; \
	  rm -f $(DESTDIR)$(consoleappdir)/$$f; \
	done
install-pixmapDATA: $(pixmap_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(pixmapdir)
	@list='$(pixmap_DATA)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " $(INSTALL_DATA) $$d$$p $(DESTDIR)$(pixmapdir)/$$f"; \
	  $(INSTALL_DATA) $$d$$p $(DESTDIR)$(pixmapdir)/$$f; \
	done

uninstall-pixmapDATA:
	@$(NORMAL_UNINSTALL)
	@list='$(pixmap_DATA)'; for p in $$list; do \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " rm -f $(DESTDIR)$(pixmapdir)/$$f"; \
	  rm -f $(DESTDIR)$(pixmapdir)/$$f; \
	done
install-pkgdataDATA: $(pkgdata_DATA)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(pkgdatadir)
	@list='$(pkgdata_DATA)'; for p in $$list; do \
	  if test -f "$$p"; then d=; else d="$(srcdir)/"; fi; \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " $(INSTALL_DATA) $$d$$p $(DESTDIR)$(pkgdatadir)/$$f"; \
	  $(INSTALL_DATA) $$d$$p $(DESTDIR)$(pkgdatadir)/$$f; \
	done

uninstall-pkgdataDATA:
	@$(NORMAL_UNINSTALL)
	@list='$(pkgdata_DATA)'; for p in $$list; do \
	  f="`echo $$p | sed -e 's|^.*/||'`"; \
	  echo " rm -f $(DESTDIR)$(pkgdatadir)/$$f"; \
	  rm -f $(DESTDIR)$(pkgdatadir)/$$f; \
	done

# This directory's subdirectories are mostly independent; you can cd
# into them and run `make' without going through this Makefile.
# To change the values of `make' variables: instead of editing Makefiles,
# (1) if the variable is set in `config.status', edit `config.status'
#     (which will cause the Makefiles to be regenerated when you run `make');
# (2) otherwise, pass the desired values on the `make' command line.
$(RECURSIVE_TARGETS):
	@set fnord $(MAKEFLAGS); amf=$$2; \
	dot_seen=no; \
	target=`echo $@ | sed s/-recursive//`; \
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  echo "Making $$target in $$subdir"; \
	  if test "$$subdir" = "."; then \
	    dot_seen=yes; \
	    local_target="$$target-am"; \
	  else \
	    local_target="$$target"; \
	  fi; \
	  (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $$local_target) \
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	done; \
	if test "$$dot_seen" = "no"; then \
	  $(MAKE) $(AM_MAKEFLAGS) "$$target-am" || exit 1; \
	fi; test -z "$$fail"

mostlyclean-recursive clean-recursive distclean-recursive \
maintainer-clean-recursive:
	@set fnord $(MAKEFLAGS); amf=$$2; \
	dot_seen=no; \
	case "$@" in \
	  distclean-* | maintainer-clean-*) list='$(DIST_SUBDIRS)' ;; \
	  *) list='$(SUBDIRS)' ;; \
	esac; \
	rev=''; for subdir in $$list; do \
	  if test "$$subdir" = "."; then :; else \
	    rev="$$subdir $$rev"; \
	  fi; \
	done; \
	rev="$$rev ."; \
	target=`echo $@ | sed s/-recursive//`; \
	for subdir in $$rev; do \
	  echo "Making $$target in $$subdir"; \
	  if test "$$subdir" = "."; then \
	    local_target="$$target-am"; \
	  else \
	    local_target="$$target"; \
	  fi; \
	  (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) $$local_target) \
	   || case "$$amf" in *=*) exit 1;; *k*) fail=yes;; *) exit 1;; esac; \
	done && test -z "$$fail"
tags-recursive:
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  test "$$subdir" = . || (cd $$subdir && $(MAKE) $(AM_MAKEFLAGS) tags); \
	done

tags: TAGS

ID: $(HEADERS) $(SOURCES) $(LISP) $(TAGS_FILES)
	list='$(SOURCES) $(HEADERS) $(TAGS_FILES)'; \
	unique=`for i in $$list; do \
	    if test -f "$$i"; then echo $$i; else echo $(srcdir)/$$i; fi; \
	  done | \
	  $(AWK) '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	mkid -fID $$unique $(LISP)

TAGS: tags-recursive $(HEADERS) $(SOURCES) config.h.in $(TAGS_DEPENDENCIES) \
		$(TAGS_FILES) $(LISP)
	tags=; \
	here=`pwd`; \
	list='$(SUBDIRS)'; for subdir in $$list; do \
	  if test "$$subdir" = .; then :; else \
	    test -f $$subdir/TAGS && tags="$$tags -i $$here/$$subdir/TAGS"; \
	  fi; \
	done; \
	list='$(SOURCES) $(HEADERS) $(TAGS_FILES)'; \
	unique=`for i in $$list; do \
	    if test -f "$$i"; then echo $$i; else echo $(srcdir)/$$i; fi; \
	  done | \
	  $(AWK) '    { files[$$0] = 1; } \
	       END { for (i in files) print i; }'`; \
	test -z "$(ETAGS_ARGS)config.h.in$$unique$(LISP)$$tags" \
	  || etags $(ETAGS_ARGS) $$tags config.h.in $$unique $(LISP)

GTAGS:
	here=`CDPATH=: && cd $(top_builddir) && pwd` \
	  && cd $(top_srcdir) \
	  && gtags -i $(GTAGS_ARGS) $$here

distclean-tags:
	-rm -f TAGS ID GTAGS GRTAGS GSYMS GPATH

DISTFILES = $(DIST_COMMON) $(DIST_SOURCES) $(TEXINFOS) $(EXTRA_DIST)

top_distdir = .
# Avoid unsightly `./'.
distdir = $(PACKAGE)-$(VERSION)

GZIP_ENV = --best

distdir: $(DISTFILES)
	-chmod -R a+w $(distdir) >/dev/null 2>&1; rm -rf $(distdir)
	mkdir $(distdir)
	$(mkinstalldirs) $(distdir)/. $(distdir)/intl $(distdir)/po
	@for file in $(DISTFILES); do \
	  if test -f $$file; then d=.; else d=$(srcdir); fi; \
	  dir=`echo "$$file" | sed -e 's,/[^/]*$$,,'`; \
	  if test "$$dir" != "$$file" && test "$$dir" != "."; then \
	    $(mkinstalldirs) "$(distdir)/$$dir"; \
	  fi; \
	  if test -d $$d/$$file; then \
	    cp -pR $$d/$$file $(distdir) \
	    || exit 1; \
	  else \
	    test -f $(distdir)/$$file \
	    || cp -p $$d/$$file $(distdir)/$$file \
	    || exit 1; \
	  fi; \
	done
	for subdir in $(SUBDIRS); do \
	  if test "$$subdir" = .; then :; else \
	    test -d $(distdir)/$$subdir \
	    || mkdir $(distdir)/$$subdir \
	    || exit 1; \
	    (cd $$subdir && \
	      $(MAKE) $(AM_MAKEFLAGS) \
	        top_distdir="$(top_distdir)" \
	        distdir=../$(distdir)/$$subdir \
	        distdir) \
	      || exit 1; \
	  fi; \
	done
	-find $(distdir) -type d ! -perm -777 -exec chmod a+rwx {} \; -o \
	  ! -type d ! -perm -444 -links 1 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -400 -exec chmod a+r {} \; -o \
	  ! -type d ! -perm -444 -exec $(SHELL) $(install_sh) -c -m a+r {} {} \; \
	|| chmod -R a+r $(distdir)
dist: distdir
	$(AMTAR) chof - $(distdir) | GZIP=$(GZIP_ENV) gzip -c >$(distdir).tar.gz
	-chmod -R a+w $(distdir) >/dev/null 2>&1; rm -rf $(distdir)

# This target untars the dist file and tries a VPATH configuration.  Then
# it guarantees that the distribution is self-contained by making another
# tarfile.
distcheck: dist
	-chmod -R a+w $(distdir) > /dev/null 2>&1; rm -rf $(distdir)
	GZIP=$(GZIP_ENV) gunzip -c $(distdir).tar.gz | $(AMTAR) xf -
	chmod -R a-w $(distdir); chmod a+w $(distdir)
	mkdir $(distdir)/=build
	mkdir $(distdir)/=inst
	chmod a-w $(distdir)
	dc_install_base=`CDPATH=: && cd $(distdir)/=inst && pwd` \
	  && cd $(distdir)/=build \
	  && ../configure --srcdir=.. --prefix=$$dc_install_base \
	    --with-included-gettext \
	  && $(MAKE) $(AM_MAKEFLAGS) \
	  && $(MAKE) $(AM_MAKEFLAGS) dvi \
	  && $(MAKE) $(AM_MAKEFLAGS) check \
	  && $(MAKE) $(AM_MAKEFLAGS) install \
	  && $(MAKE) $(AM_MAKEFLAGS) installcheck \
	  && $(MAKE) $(AM_MAKEFLAGS) uninstall \
	  && (test `find $$dc_install_base -type f -print | wc -l` -le 1 \
	     || (echo "Error: files left after uninstall" 1>&2; \
	         exit 1) ) \
	  && $(MAKE) $(AM_MAKEFLAGS) dist \
	  && $(MAKE) $(AM_MAKEFLAGS) distclean \
	  && rm -f $(distdir).tar.gz \
	  && (test `find . -type f -print | wc -l` -eq 0 \
	     || (echo "Error: files left after distclean" 1>&2; \
	         exit 1) )
	-chmod -R a+w $(distdir) > /dev/null 2>&1; rm -rf $(distdir)
	@echo "$(distdir).tar.gz is ready for distribution" | \
	  sed 'h;s/./=/g;p;x;p;x'
check-am: all-am
check: check-recursive
all-am: Makefile $(PROGRAMS) $(SCRIPTS) $(MANS) $(DATA) config.h
installdirs: installdirs-recursive
installdirs-am:
	$(mkinstalldirs) $(DESTDIR)$(bindir) $(DESTDIR)$(sbindir) $(DESTDIR)$(bindir) $(DESTDIR)$(man1dir) $(DESTDIR)$(man8dir) $(DESTDIR)$(applnkdir) $(DESTDIR)$(consoleappdir) $(DESTDIR)$(pixmapdir) $(DESTDIR)$(pkgdatadir)

install: install-recursive
install-exec: install-exec-recursive
install-data: install-data-recursive
uninstall: uninstall-recursive

install-am: all-am
	@$(MAKE) $(AM_MAKEFLAGS) install-exec-am install-data-am

installcheck: installcheck-recursive
install-strip:
	$(MAKE) $(AM_MAKEFLAGS) INSTALL_PROGRAM="$(INSTALL_STRIP_PROGRAM)" \
	  `test -z '$(STRIP)' || \
	    echo "INSTALL_PROGRAM_ENV=STRIPPROG='$(STRIP)'"` install
mostlyclean-generic:

clean-generic:

distclean-generic:
	-rm -f Makefile $(CONFIG_CLEAN_FILES) stamp-h stamp-h[0-9]*

maintainer-clean-generic:
	@echo "This command is intended for maintainers to use"
	@echo "it deletes files that may require special tools to rebuild."
clean: clean-recursive

clean-am: clean-binPROGRAMS clean-generic clean-noinstPROGRAMS \
	clean-sbinPROGRAMS mostlyclean-am

dist-all: distdir
	$(AMTAR) chof - $(distdir) | GZIP=$(GZIP_ENV) gzip -c >$(distdir).tar.gz
	-chmod -R a+w $(distdir) >/dev/null 2>&1; rm -rf $(distdir)
distclean: distclean-recursive
	-rm -f config.status config.cache config.log
distclean-am: clean-am distclean-compile distclean-depend \
	distclean-generic distclean-hdr distclean-tags

dvi: dvi-recursive

dvi-am:

info: info-recursive

info-am:

install-data-am: install-applnkDATA install-consoleappDATA install-man \
	install-pixmapDATA install-pkgdataDATA

install-exec-am: install-binPROGRAMS install-binSCRIPTS \
	install-sbinPROGRAMS

install-info: install-info-recursive

install-man: install-man1 install-man8

installcheck-am:

maintainer-clean: maintainer-clean-recursive

maintainer-clean-am: distclean-am maintainer-clean-generic

mostlyclean: mostlyclean-recursive

mostlyclean-am: mostlyclean-compile mostlyclean-generic

uninstall-am: uninstall-applnkDATA uninstall-binPROGRAMS \
	uninstall-binSCRIPTS uninstall-consoleappDATA uninstall-info-am \
	uninstall-man uninstall-pixmapDATA uninstall-pkgdataDATA \
	uninstall-sbinPROGRAMS

uninstall-info: uninstall-info-recursive

uninstall-man: uninstall-man1 uninstall-man8

.PHONY: $(RECURSIVE_TARGETS) GTAGS all all-am check check-am clean \
	clean-binPROGRAMS clean-generic clean-noinstPROGRAMS \
	clean-recursive clean-sbinPROGRAMS dist dist-all distcheck \
	distclean distclean-compile distclean-depend distclean-generic \
	distclean-hdr distclean-recursive distclean-tags distdir dvi \
	dvi-am dvi-recursive info info-am info-recursive install \
	install-am install-applnkDATA install-binPROGRAMS \
	install-binSCRIPTS install-consoleappDATA install-data \
	install-data-am install-data-recursive install-exec \
	install-exec-am install-exec-recursive install-info \
	install-info-am install-info-recursive install-man install-man1 \
	install-man8 install-pixmapDATA install-pkgdataDATA \
	install-recursive install-sbinPROGRAMS install-strip \
	installcheck installcheck-am installdirs installdirs-am \
	installdirs-recursive maintainer-clean maintainer-clean-generic \
	maintainer-clean-recursive mostlyclean mostlyclean-compile \
	mostlyclean-generic mostlyclean-recursive tags tags-recursive \
	uninstall uninstall-am uninstall-applnkDATA \
	uninstall-binPROGRAMS uninstall-binSCRIPTS \
	uninstall-consoleappDATA uninstall-info-am \
	uninstall-info-recursive uninstall-man uninstall-man1 \
	uninstall-man8 uninstall-pixmapDATA uninstall-pkgdataDATA \
	uninstall-recursive uninstall-sbinPROGRAMS

$(WRAPPED_APPS): /dev/null
	$(INSTALL) -m600 $^ $@

tag:
	cvs tag -cR $(CVSTAG) .
# Tell versions [3.59,3.63) of GNU make to not export all variables.
# Otherwise a system limit (for SysV at least) may be exceeded.
.NOEXPORT:
