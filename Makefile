# mostly and totally crappy makefile... better one to come.
VERSION=$(shell awk '/^Version:/ { print $$2 }' < usermode.spec)
REVISION=$(shell awk '/^Revision:/ { print $$2 }' < usermode.spec)
CVSTAG = um$(subst .,-,$(VERSION))$(subst .,-,$(REVISION))

#CFLAGS=-O2 -Wall
CFLAGS=-g -Wall $(shell gtk-config --cflags)
LDFLAGS=$(shell gtk-config --libs)
INSTALL=install

PROGS=userinfo usermount userhelper userpasswd consolehelper
MANS=userinfo.1 usermount.1 userhelper.8 userpasswd.1 consolehelper.8

all: 	$(PROGS)

userhelper:	userhelper.c
	$(CC) -ouserhelper $(CFLAGS) userhelper.c -lpwdb -lpam -lpam_misc -ldl

test:	test-userdialog

test-userdialog: 	userdialogs.o test-userdialog.o
	$(CC) -otest-userdialog $(CFLAGS) userdialogs.o test-userdialog.o $(LDFLAGS)

userinfo:	userinfo.o userdialogs.o userhelper-wrap.o
	$(CC) -ouserinfo $(CFLAGS) userinfo.o userdialogs.o userhelper-wrap.o $(LDFLAGS)

usermount:	usermount.o userdialogs.o
	$(CC) -ousermount $(CFLAGS) usermount.o userdialogs.o $(LDFLAGS)

userpasswd:	userpasswd.o userdialogs.o userhelper-wrap.o
	$(CC) -ouserpasswd $(CFLAGS) userpasswd.o userdialogs.o userhelper-wrap.o $(LDFLAGS)

consolehelper:	consolehelper.o userdialogs.o userhelper-wrap.o
	$(CC) -oconsolehelper $(CFLAGS) consolehelper.o userdialogs.o userhelper-wrap.o $(LDFLAGS)

install:	$(PROGS)
	mkdir -p $(PREFIX)/usr/bin $(PREFIX)/usr/sbin
	mkdir -p $(PREFIX)/etc/X11/applink/System
	mkdir -p $(PREFIX)/usr/man/man1 $(PREFIX)/usr/man/man8
	$(INSTALL) -m 755 -s userinfo $(PREFIX)/usr/bin
	$(INSTALL) -m 644 userinfo.desktop $(PREFIX)/etc/X11/applink/System/
	$(INSTALL) -m 755 -s usermount $(PREFIX)/usr/bin
	$(INSTALL) -m 644 usermount.desktop $(PREFIX)/etc/X11/applink/System/
	$(INSTALL) -m 755 -s userpasswd $(PREFIX)/usr/bin
	$(INSTALL) -m 644 userpasswd.desktop $(PREFIX)/etc/X11/applink/System/
	$(INSTALL) -m 755 -s consolehelper $(PREFIX)/usr/bin
	$(INSTALL) -m 4755 -s userhelper $(PREFIX)/usr/sbin

install-man: 	$(MANS)
	$(INSTALL) -m 644 userinfo.1 $(PREFIX)/usr/man/man1
	$(INSTALL) -m 644 usermount.1 $(PREFIX)/usr/man/man1
	$(INSTALL) -m 644 userhelper.8 $(PREFIX)/usr/man/man8
	$(INSTALL) -m 644 consolehelper.8 $(PREFIX)/usr/man/man8
	$(INSTALL) -m 644 userpasswd.1 $(PREFIX)/usr/man/man1

clean:	
	rm -f *~ *.o $(PROGS)

archive:
	cvs tag -F $(CVSTAG) .
	@rm -rf /tmp/usermode-$(VERSION) /tmp/usermode
	@cd /tmp; cvs export -r$(CVSTAG) usermode
	@mv /tmp/usermode /tmp/usermode-$(VERSION)
	@dir=$$PWD; cd /tmp; tar cvzf $$dir/usermode-$(VERSION).tar.gz usermode-$(VERSION)
	@rm -rf /tmp/usermode-$(VERSION)
	@echo "The archive is in usermode-$(VERSION).tar.gz"
