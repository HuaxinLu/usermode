VERSION=$(shell awk '/^Version:/ { print $$2 }' < usermode.spec)
RELEASE=$(shell awk '/^Release:/ { print $$2 }' < usermode.spec)
CVSTAG = usermode-$(subst .,-,$(VERSION)-$(RELEASE))
EXTRA_DIST = config.rpath  \
	dummy.h \
	usermode.spec \
	shutdown \
	userinfo.1 \
	usermount.1 \
	userpasswd.1 \
	consolehelper.8 \
	userhelper.8 \
	userinfo.desktop \
	usermount.desktop \
	userpasswd.desktop \
	keys.xpm \
	usermode.glade \
	shutdown.pamd \
	shutdown.pamd.6x \
	update-from-egg.sh \
	eggmarshalers.list \
	$(pixmap_DATA)

SUBDIRS=m4 po

CFLAGS = @LIBGLADE_CFLAGS@ @LIBUSER_CFLAGS@ @CFLAGS@ -DDATADIR=\"$(datadir)\" -DGETTEXT_PACKAGE=\"$(PACKAGE)\"

bin_PROGRAMS = userinfo usermount userpasswd consolehelper consolehelper-gtk pam-panel-icon
bin_SCRIPTS = shutdown
sbin_PROGRAMS = userhelper
noinst_PROGRAMS = test-userdialog
man1_MANS = userinfo.1 usermount.1 userpasswd.1
man8_MANS = consolehelper.8 userhelper.8
install-data-local:
	$(INSTALL) -m755 -d $(DESTDIR)/$(mandir)/man1
	$(INSTALL) -m644 $(man1_MANS) $(DESTDIR)/$(mandir)/man1/
	$(INSTALL) -m755 -d $(DESTDIR)/$(mandir)/man8
	$(INSTALL) -m644 $(man8_MANS) $(DESTDIR)/$(mandir)/man8/

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = keys.xpm status_lock.png

applnkdir = $(sysconfdir)/X11/applnk/System
applnk_DATA = userinfo.desktop usermount.desktop userpasswd.desktop

consoleappdir = $(sysconfdir)/security/console.apps
WRAPPED_APPS = halt poweroff reboot
consoleapp_DATA = $(WRAPPED_APPS)
$(WRAPPED_APPS): /dev/null
	$(INSTALL) -m600 $^ $@

pkgdata_DATA = usermode.glade

userhelper_SOURCES = userhelper.c userhelper.h shvar.c shvar.h
userhelper_LDADD = @LIBUSER_LIBS@ @GLIB_LIBS@ @PAM_LIBS@ @LIBS@

userinfo_SOURCES = userinfo.c userdialogs.c userhelper-wrap.c userhelper-wrap.h
userinfo_LDADD = @LIBGLADE_LIBS@ @LIBS@

usermount_SOURCES = usermount.c userdialogs.c
usermount_LDADD = @LIBGLADE_LIBS@ @LIBS@

userpasswd_SOURCES = userpasswd.c userdialogs.c userdialogs.h userhelper-wrap.c
userpasswd_LDADD = @LIBGLADE_LIBS@ @LIBS@

# Aaagh! I want automake 1.5!
consolehelper_CFLAGS = -DDISABLE_X11 $(CFLAGS)
consolehelper_SOURCES = consolehelper-nox.c
consolehelper_LDADD = @GLIB_LIBS@ @LIBS@

consolehelper_gtk_SOURCES = consolehelper.c userdialogs.c userhelper-wrap.c
consolehelper_gtk_LDADD = @LIBGLADE_LIBS@ @LIBS@

pam_panel_icon_SOURCES = pam-panel-icon.c $(EGGFILES) egg-marshal.c gsmclient.c gsmclient.h props.h props.c
pam_panel_icon_LDADD = @LIBGLADE_LIBS@ @LIBS@ -L/usr/X11R6/lib -lICE -lSM
pam-panel-icon.c: egg-marshal.c

test_userdialog_SOURCES = test-userdialog.c userdialogs.c
test_userdialog_LDADD = @LIBGLADE_LIBS@ @LIBS@

tag:
	cvs tag -cFR $(CVSTAG) .
	rm -fr /tmp/$(PACKAGE)-export-tmp
	CVSROOT=`cat CVS/Root`; cd /tmp; cvs -d $$CVSROOT export -r $(CVSTAG) -d $(PACKAGE)-export-tmp $(PACKAGE)
	cd /tmp/$(PACKAGE)-export-tmp; ./autogen.sh ; make dist
	mv /tmp/$(PACKAGE)-export-tmp/$(PACKAGE)-$(VERSION).tar.gz .
	rm -fr /tmp/$(PACKAGE)-export-tmp

ACLOCAL_AMFLAGS = -I m4

## libegg goo - "make regenerate-built-sources EGGDIR=foo"
EGGFILES=			\
	eggtrayicon.h		\
	eggtrayicon.c

EGGDIR="you need to set EGGDIR variable"

regenerate-built-sources:
	EGGFILES="$(EGGFILES)" EGGDIR="$(EGGDIR)" $(srcdir)/update-from-egg.sh

eggmarshalers.h:
	cd $(srcdir)										\
	&& $(GLIB_GENMARSHAL) --prefix=_egg_marshal eggmarshalers.list --header > xgen-emh	\
	&& cp xgen-emh eggmarshalers.h								\
	&& rm -f xgen-emh xgen-emh~

eggmarshalers.c:
	cd $(srcdir)										\
	&& $(GLIB_GENMARSHAL) --prefix=_egg_marshal eggmarshalers.list --body > xgen-emc	\
	&& cp xgen-emc eggmarshalers.c								\
	&& rm -f xgen-emc xgen-emc~

egg-marshal.c: eggmarshalers.h eggmarshalers.c
