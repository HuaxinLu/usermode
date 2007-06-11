CVSTAG = usermode-$$(echo '$(VERSION)' | tr '.' '_')
VENDOR=redhat

EXTRA_DIST = 	\
	dummy.h \
	shutdown \
	userinfo.1 \
	usermount.1 \
	userpasswd.1 \
	consolehelper.8 \
	userhelper.8 \
	userhelper_context \
	userinfo.desktop.in \
	usermount.desktop.in \
	userpasswd.desktop.in \
	wrapped.console \
	keys.xpm \
	usermode.glade \
	shutdown.pamd \
	admin/intltool-extract.in \
	admin/intltool-merge.in \
	admin/intltool-update.in \
	$(pixmap_DATA)

SUBDIRS=po

AM_CPPFLAGS = -DLOCALEDIR='"$(localedir)"'
AM_CFLAGS = $(SN_WNCK_CFLAGS) $(LIBGLADE_CFLAGS) $(LIBUSER_CFLAGS)
BINDIR_CPPFLAGS = -DBINDIR='"$(bindir)"'
PIXMAPDIR_CPPFLAGS = -DPIXMAPDIR='"$(pixmapdir)"'
SBINDIR_CPPFLAGS = -DSBINDIR='"$(sbindir)"'

bin_PROGRAMS = userinfo usermount userpasswd consolehelper consolehelper-gtk \
	pam-panel-icon
bin_SCRIPTS = shutdown
sbin_PROGRAMS = userhelper
noinst_PROGRAMS = test-userdialog
man_MANS = userinfo.1 usermount.1 userpasswd.1 consolehelper.8 userhelper.8

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = keys.xpm status_lock.png status_unlocked.png user_icon.png \
	password.png disks.png keyring.png badge-small.png

install-data-local: userinfo.desktop usermount.desktop userpasswd.desktop
	mkdir -p $(DESTDIR)$(datadir)/applications
	desktop-file-install --vendor=$(VENDOR) \
		--add-category Settings \
		--add-category AdvancedSettings \
		--add-category Application \
		--dir=$(DESTDIR)$(datadir)/applications \
		$(top_builddir)/userinfo.desktop
	desktop-file-install --vendor=$(VENDOR) \
		--add-category X-Red-Hat-Base \
		--add-category System \
		--add-category Application \
		--dir=$(DESTDIR)$(datadir)/applications \
		$(top_builddir)/usermount.desktop
	desktop-file-install --vendor=$(VENDOR) \
		--add-category X-Red-Hat-Base \
		--add-category Settings \
		--add-category Application \
		--dir=$(DESTDIR)$(datadir)/applications \
		$(top_builddir)/userpasswd.desktop

uninstall-local:
	-rm -f $(DESTDIR)$(datadir)/applications/$(VENDOR)-userinfo.desktop   \
	       $(DESTDIR)$(datadir)/applications/$(VENDOR)-usermount.desktop  \
	       $(DESTDIR)$(datadir)/applications/$(VENDOR)-userpasswd.desktop 

consoleappdir = $(sysconfdir)/security/console.apps
WRAPPED_APPS = halt poweroff reboot
consoleapp_DATA = $(WRAPPED_APPS)
$(WRAPPED_APPS): wrapped.console
	$(INSTALL) -m600 $^ $@

DISTCLEANFILES =            \
	intltool-extract    \
	intltool-merge      \
	intltool-update     \
	userinfo.desktop    \
	usermount.desktop   \
	userpasswd.desktop  \
	$(WRAPPED_APPS)     


debug-install:
	$(INSTALL) -m4711 userhelper $(sbindir)/
	$(INSTALL)  -m755 consolehelper $(bindir)/
	$(INSTALL)  -m755 consolehelper-gtk $(bindir)/
	$(INSTALL)  -m755 userinfo          $(bindir)/
	$(INSTALL)  -m755 usermount         $(bindir)/
	$(INSTALL)  -m755 userpasswd        $(bindir)/

pkgdata_DATA = usermode.glade

@INTLTOOL_DESKTOP_RULE@

userhelper_SOURCES = userhelper.c userhelper.h shvar.c shvar.h
userhelper_CPPFLAGS = $(AM_CPPFLAGS) -DSYSCONFDIR='"$(sysconfdir)"'
userhelper_LDADD = $(LIBUSER_LIBS) $(GLIB_LIBS) $(PAM_LIBS) $(SELINUX_LIBS) $(EFENCE_LIBS)

userinfo_SOURCES = userinfo.c userdialogs.c userdialogs.h userhelper-wrap.c \
	userhelper-wrap.h
userinfo_CPPFLAGS = $(AM_CPPFLAGS) $(PIXMAPDIR_CPPFLAGS) $(SBINDIR_CPPFLAGS) \
	-DPKGDATADIR='"$(pkgdatadir)"'
userinfo_LDADD = $(SN_WNCK_LIBS) $(LIBGLADE_LIBS) $(EFENCE_LIBS)

usermount_SOURCES = usermount.c userdialogs.c userdialogs.h
usermount_CPPFLAGS = $(AM_CPPFLAGS) $(PIXMAPDIR_CPPFLAGS)
usermount_LDADD = $(GTK_LIBS) -lblkid $(EFENCE_LIBS)

userpasswd_SOURCES = userpasswd.c userdialogs.c userdialogs.h \
	userhelper-wrap.c userhelper-wrap.h
userpasswd_CPPFLAGS = $(AM_CPPFLAGS) $(PIXMAPDIR_CPPFLAGS) $(SBINDIR_CPPFLAGS)
userpasswd_LDADD = $(SN_WNCK_LIBS) $(GTK_LIBS) $(EFENCE_LIBS)

consolehelper_SOURCES = consolehelper-nox.c
consolehelper_CPPFLAGS = $(AM_CPPFLAGS) $(BINDIR_CPPFLAGS) $(SBINDIR_CPPFLAGS)
consolehelper_LDADD = $(GLIB_LIBS) $(EFENCE_LIBS)

consolehelper_gtk_SOURCES = consolehelper.c userdialogs.c userdialogs.h \
	userhelper-wrap.c userhelper-wrap.h
consolehelper_gtk_CPPFLAGS = $(AM_CPPFLAGS) $(PIXMAPDIR_CPPFLAGS) \
	$(SBINDIR_CPPFLAGS)
consolehelper_gtk_LDADD = $(SN_WNCK_LIBS) $(GTK_LIBS) $(EFENCE_LIBS)

pam_panel_icon_SOURCES = pam-panel-icon.c gsmclient.c gsmclient.h props.c \
	props.h
pam_panel_icon_CPPFLAGS = $(AM_CPPFLAGS) $(BINDIR_CPPFLAGS) \
	$(PIXMAPDIR_CPPFLAGS) -DGETTEXT_PACKAGE=\"$(PACKAGE)\"
pam_panel_icon_LDADD = $(GTK_LIBS) $(X_LIBS) -lICE -lSM $(EFENCE_LIBS)

test_userdialog_SOURCES = test-userdialog.c userdialogs.c userdialogs.h
test_userdialog_LDADD = $(GTK_LIBS) $(EFENCE_LIBS)

consolehelper.8: $(srcdir)/consolehelper.8.in Makefile
	sed 's,@''BINDIR@,$(bindir),g' < $(srcdir)/consolehelper.8.in > $@

userhelper.8: $(srcdir)/userhelper.8.in Makefile
	sed 's,@''SYSCONFDIR@,$(sysconfdir),g' < $(srcdir)/userhelper.8.in > $@

tag:
	cd $(top_srcdir); cvs tag -cR $(CVSTAG) .

forcetag:
	cd $(top_srcdir); cvs tag -cFR $(CVSTAG) .

archive:
	rm -fr /tmp/$(PACKAGE)-export-tmp
	CVSROOT=`cat $(top_srcdir)/CVS/Root`; cd /tmp; cvs -d $$CVSROOT \
		export -r $(CVSTAG) -d $(PACKAGE)-export-tmp $(PACKAGE)
	cd /tmp/$(PACKAGE)-export-tmp; ./autogen.sh ; \
		./configure --sysconfdir=/etc --prefix=/usr; make dist
	mv /tmp/$(PACKAGE)-export-tmp/$(PACKAGE)-$(VERSION).tar.bz2 .
	rm -fr /tmp/$(PACKAGE)-export-tmp
