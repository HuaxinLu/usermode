VERSION=$(shell awk '/^Version:/ { print $$2 }' < usermode.spec)
RELEASE=$(shell awk '/^Release:/ { print $$2 }' < usermode.spec)
CVSTAG = usermode-$(subst .,-,$(VERSION)-$(RELEASE))

SUBDIRS=intl po

CFLAGS = @LIBGLADE_CFLAGS@ @CFLAGS@
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
$(WRAPPED_APPS): /dev/null
	$(INSTALL) -m600 $^ $@

pkgdata_DATA = usermode.glade

userhelper_SOURCES = userhelper.c shvar.c
userhelper_LDADD = @GLIB_LIBS@ @PAM_LIBS@ @LIBS@

userinfo_SOURCES = userinfo.c userdialogs.c userhelper-wrap.c
userinfo_LDADD = @LIBGLADE_LIBS@ @LIBS@

usermount_SOURCES = usermount.c userdialogs.c
usermount_LDADD = @LIBGLADE_LIBS@ @LIBS@

userpasswd_SOURCES = userpasswd.c userdialogs.c userhelper-wrap.c
userpasswd_LDADD = @LIBGLADE_LIBS@ @LIBS@

consolehelper_SOURCES = consolehelper.c
consolehelper_LDADD = @GLIB_LIBS@ @LIBS@

consolehelper_x11_SOURCES = consolehelper.c userdialogs.c userhelper-wrap.c
consolehelper_x11_LDADD = @LIBGLADE_LIBS@ @LIBS@

test_userdialog_SOURCES = test-userdialog.c userdialogs.c
test_userdialog_LDADD = @LIBGLADE_LIBS@ @LIBS@

tag:
	cvs tag -cR $(CVSTAG) .

archive:
	@rm -rf /tmp/usermode-$(VERSION) /tmp/usermode
	@CVSROOT=`cat CVS/Root`; cd /tmp; cvs -d $$CVSROOT export -r$(CVSTAG) usermode
	@mv /tmp/usermode /tmp/usermode-$(VERSION)
	@dir=$$PWD; cd /tmp; tar cvjf $$dir/usermode-$(VERSION).tar.bz2 usermode-$(VERSION)
	@rm -rf /tmp/usermode-$(VERSION)
	@echo "The archive is in usermode-$(VERSION).tar.bz2"
