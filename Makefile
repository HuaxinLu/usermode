CC=gcc

# mostly and totally crappy makefile... better one to come.
VERSION=$(shell awk '/^Version:/ { print $$2 }' < usermode.spec)
REVISION=$(shell awk '/^Revision:/ { print $$2 }' < usermode.spec)
CVSTAG = um$(subst .,-,$(VERSION))$(subst .,-,$(REVISION))

#CFLAGS=-O2 -Wall
CFLAGS=-g -Wall $(shell gtk-config --cflags)
LDFLAGS=$(shell gtk-config --libs)
INSTALL=install

prefix=/usr
bindir=$(prefix)/bin
mandir=$(prefix)/man
sbindir=$(prefix)/sbin
datadir=$(prefix)/share

PROGS=userinfo usermount userhelper userpasswd consolehelper
MANS=userinfo.1 usermount.1 userhelper.8 userpasswd.1 consolehelper.8

all: $(PROGS)

userhelper: userhelper.o shvar.o
	$(CC) -ouserhelper $(CFLAGS) $^ -lglib -lpwdb -lpam -lpam_misc -ldl

test:	test-userdialog

test-userdialog:  userdialogs.o test-userdialog.o
	$(CC) -otest-userdialog $(CFLAGS) $^ $(LDFLAGS)

userinfo: userinfo.o userdialogs.o userhelper-wrap.o
	$(CC) -ouserinfo $(CFLAGS) $^ $(LDFLAGS)

usermount: usermount.o userdialogs.o
	$(CC) -ousermount $(CFLAGS) $^ $(LDFLAGS)

userpasswd: userpasswd.o userdialogs.o userhelper-wrap.o
	$(CC) -ouserpasswd $(CFLAGS) $^ $(LDFLAGS)

consolehelper: consolehelper.o userdialogs.o userhelper-wrap.o
	$(CC) -oconsolehelper $(CFLAGS) $^ $(LDFLAGS)

install:	$(PROGS)
	mkdir -p $(PREFIX)$(bindir) $(PREFIX)$(sbindir)
	mkdir -p $(PREFIX)/etc/X11/applnk/System
	mkdir -p $(PREFIX)$(datadir)/pixmaps
	mkdir -p $(PREFIX)$(mandir)/man1 $(PREFIX)$(mandir)/man8
	mkdir -p $(PREFIX)$(mandir)/man1 $(PREFIX)$(mandir)/man8
	$(INSTALL) -m 755 userinfo $(PREFIX)$(bindir)
	$(INSTALL) -m 644 userinfo.desktop $(PREFIX)/etc/X11/applnk/System/
	$(INSTALL) -m 755 usermount $(PREFIX)$(bindir)
	$(INSTALL) -m 644 usermount.desktop $(PREFIX)/etc/X11/applnk/System/
	$(INSTALL) -m 755 userpasswd $(PREFIX)$(bindir)
	$(INSTALL) -m 644 userpasswd.desktop $(PREFIX)/etc/X11/applnk/System/
	$(INSTALL) -m 755 consolehelper $(PREFIX)$(bindir)
	$(INSTALL) -m 4755 userhelper $(PREFIX)$(sbindir)
	$(INSTALL) -m 644 keys.xpm $(PREFIX)$(datadir)/pixmaps/userhelper-keys.xpm

install-man: 	$(MANS)
	$(INSTALL) -m 644 userinfo.1 $(PREFIX)$(mandir)/man1
	$(INSTALL) -m 644 usermount.1 $(PREFIX)$(mandir)/man1
	$(INSTALL) -m 644 userhelper.8 $(PREFIX)$(mandir)/man8
	$(INSTALL) -m 644 consolehelper.8 $(PREFIX)$(mandir)/man8
	$(INSTALL) -m 644 userpasswd.1 $(PREFIX)$(mandir)/man1

install-po:
	$(MAKE) -C po $@ PREFIX=$(PREFIX) datadir=$(datadir) prefix=$(prefix) bindir=$(prefix)/bin mandir=$(prefix)/man sbindir=$(prefix)/sbin datadir=$(prefix)/share

clean:	
	rm -f *~ *.o $(PROGS)

archive:
	cvs tag -F $(CVSTAG) .
	@rm -rf /tmp/usermode-$(VERSION) /tmp/usermode
	@cd /tmp; cvs export -r$(CVSTAG) usermode
	@mv /tmp/usermode /tmp/usermode-$(VERSION)
	@dir=$$PWD; cd /tmp; tar cvIf $$dir/usermode-$(VERSION).tar.bz2 usermode-$(VERSION)
	@rm -rf /tmp/usermode-$(VERSION)
	@echo "The archive is in usermode-$(VERSION).tar.bz2"
