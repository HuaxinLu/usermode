# completely and totally crappy makefile... better one to come.

CFLAGS=-O2 -Wall
INCLUDES=
LDFLAGS=-L/usr/X11R6/lib -lm -lX11 -lXext -lglib -lgdk -lgtk 
INSTALL=install

PROGS=userinfo usermount userhelper userpasswd
MANS=userinfo.1 usermount.1 userhelper.8 userpasswd.1

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

install:	$(PROGS)
	$(INSTALL) -m 755 -o root -g root userinfo /usr/bin
	$(INSTALL) -m 755 -o root -g root userinfo.wmconfig /etc/X11/wmconfig/userinfo
	$(INSTALL) -m 755 -o root -g root usermount /usr/bin
	$(INSTALL) -m 755 -o root -g root usermount.wmconfig /etc/X11/wmconfig/usermount
	$(INSTALL) -m 755 -o root -g root userpasswd /usr/bin
	$(INSTALL) -m 755 -o root -g root userpasswd.wmconfig /etc/X11/wmconfig/userpasswd
	$(INSTALL) -m 755 -o root -g root userhelper /usr/sbin

install-man: 	$(MANS)
	$(INSTALL) -m 755 -o root -g root userinfo.1 /usr/man/man1
	$(INSTALL) -m 755 -o root -g root usermount.1 /usr/man/man1
	$(INSTALL) -m 755 -o root -g root userhelper.8 /usr/man/man8
	$(INSTALL) -m 755 -o root -g root userpasswd.1 /usr/man/man1

install-wmconfig:	$(WMCONFIG)
	$(INSTALL) -m 644 -o root -g root userinfo.wmconfig /etc/X11/wmconfig/userinfo
	$(INSTALL) -m 644 -o root -g root userpasswd.wmconfig /etc/X11/wmconfig/userpasswd
	$(INSTALL) -m 644 -o root -g root usermount.wmconfig /etc/X11/wmconfig/usermount

clean:	
	rm -f *~ *.o $(PROGS)
