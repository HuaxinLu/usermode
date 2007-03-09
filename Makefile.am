CVSTAG = usermode-$$(echo '$(VERSION)' | tr '.' '_')
VENDOR=redhat

BUILT_SOURCES = marshal.c marshal.h
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
	marshal.list \
	pam_timestamp_init.console \
	pam_timestamp_init.pamd \
	wrapped.console \
	keys.xpm \
	usermode.glade \
	shutdown.pamd \
	shutdown.pamd.6x \
	update-from-egg.sh \
	eggmarshalers.list \
	admin/intltool-extract.in \
	admin/intltool-merge.in \
	admin/intltool-update.in \
	$(pixmap_DATA)

SUBDIRS=po

AM_CFLAGS = @SN_WNCK_CFLAGS@ @LIBGLADE_CFLAGS@ @LIBUSER_CFLAGS@ @CFLAGS@ -DDATADIR=\"$(datadir)\" -DGETTEXT_PACKAGE=\"$(PACKAGE)\"

bin_PROGRAMS = userinfo usermount userpasswd consolehelper consolehelper-gtk pam-panel-icon
bin_SCRIPTS = shutdown
sbin_PROGRAMS = userhelper
noinst_PROGRAMS = test-userdialog
man_MANS = userinfo.1 usermount.1 userpasswd.1 consolehelper.8 userhelper.8

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = keys.xpm status_lock.png status_unlocked.png user_icon.png password.png disks.png keyring.png badge-small.png

install-data-local: userinfo.desktop usermount.desktop userpasswd.desktop
	mkdir -p $(DESTDIR)/$(datadir)/applications
	desktop-file-install --vendor=$(VENDOR) \
		--add-category Settings \
		--add-category AdvancedSettings \
		--add-category Application \
		--dir=$(DESTDIR)/$(datadir)/applications \
		$(top_builddir)/userinfo.desktop
	desktop-file-install --vendor=$(VENDOR) \
		--add-category X-Red-Hat-Base \
		--add-category System \
		--add-category Application \
		--dir=$(DESTDIR)/$(datadir)/applications \
		$(top_builddir)/usermount.desktop
	desktop-file-install --vendor=$(VENDOR) \
		--add-category X-Red-Hat-Base \
		--add-category Settings \
		--add-category Application \
		--dir=$(DESTDIR)/$(datadir)/applications \
		$(top_builddir)/userpasswd.desktop

uninstall-local: 
	-rm -f $(DESTDIR)/$(datadir)/applications/$(VENDOR)-userinfo.desktop   \
	       $(DESTDIR)/$(datadir)/applications/$(VENDOR)-usermount.desktop  \
	       $(DESTDIR)/$(datadir)/applications/$(VENDOR)-userpasswd.desktop 

consoleappdir = $(sysconfdir)/security/console.apps
WRAPPED_APPS = halt poweroff reboot
INIT_APPS = pam_timestamp_init
consoleapp_DATA = $(WRAPPED_APPS)
$(WRAPPED_APPS): wrapped.console
	$(INSTALL) -m600 $^ $@
        
$(INIT_APPS): pam_timestamp_init.console
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
userhelper_LDADD = @LIBUSER_LIBS@ @GLIB_LIBS@ @PAM_LIBS@ @LIBS@ @EFENCE_LIBS@

userinfo_SOURCES = userinfo.c userdialogs.c userdialogs.h userhelper-wrap.c userhelper-wrap.h reaper.c reaper.h marshal.c marshal.h
userinfo_LDADD = @SN_WNCK_LIBS@ @LIBGLADE_LIBS@ @LIBS@ @EFENCE_LIBS@

usermount_SOURCES = usermount.c userdialogs.c
usermount_LDADD = @LIBGLADE_LIBS@ @LIBS@ @EFENCE_LIBS@

userpasswd_SOURCES = userpasswd.c userdialogs.c userdialogs.h userhelper-wrap.c reaper.c userhelper-wrap.h reaper.h marshal.c marshal.h
userpasswd_LDADD = @SN_WNCK_LIBS@ @LIBGLADE_LIBS@ @LIBS@ @EFENCE_LIBS@

consolehelper_SOURCES = consolehelper-nox.c
consolehelper_LDADD = @GLIB_LIBS@ @LIBS@ @EFENCE_LIBS@

consolehelper_gtk_SOURCES = consolehelper.c userdialogs.c userdialogs.h userhelper-wrap.c userhelper-wrap.h reaper.c reaper.h marshal.c marshal.h
consolehelper_gtk_LDADD = @SN_WNCK_LIBS@ @LIBGLADE_LIBS@ @LIBS@ @EFENCE_LIBS@

pam_panel_icon_SOURCES = pam-panel-icon.c marshal.c gsmclient.c gsmclient.h props.h props.c
pam_panel_icon_LDADD = @LIBGLADE_LIBS@ @LIBS@ $(X_LIBS) -lICE -lSM @EFENCE_LIBS@

if !HAVE_GTK210
pam_panel_icon_SOURCES += eggtrayicon.c eggtrayicon.h
endif

test_userdialog_SOURCES = test-userdialog.c userdialogs.c userdialogs.h
test_userdialog_LDADD = @LIBGLADE_LIBS@ @LIBS@ @EFENCE_LIBS@

tag:
	cd $(top_srcdir); cvs tag -cR $(CVSTAG) .

forcetag:
	cd $(top_srcdir); cvs tag -cFR $(CVSTAG) .

archive:
	rm -fr /tmp/$(PACKAGE)-export-tmp
	CVSROOT=`cat $(top_srcdir)/CVS/Root`; cd /tmp; cvs -d $$CVSROOT \
		export -r $(CVSTAG) -d $(PACKAGE)-export-tmp $(PACKAGE)
	cd /tmp/$(PACKAGE)-export-tmp; ./autogen.sh ; \
		./configure --sysconfdir=etc --prefix=/usr; make dist
	mv /tmp/$(PACKAGE)-export-tmp/$(PACKAGE)-$(VERSION).tar.bz2 .
	rm -fr /tmp/$(PACKAGE)-export-tmp

marshal.c marshal.h: marshal.list
	glib-genmarshal --prefix=_vte_marshal --body $^ > marshal.c
	glib-genmarshal --prefix=_vte_marshal --header $^ > marshal.h

## libegg goo - "make regenerate-built-sources EGGDIR=foo"
EGGFILES=			\
	eggtrayicon.h		\
	eggtrayicon.c

EGGDIR="you need to set EGGDIR variable"

regenerate-built-sources:
	EGGFILES="$(EGGFILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh
