Summary: User Tools
Name: usermode
Version: 1.4
Release: 1
Copyright: GPL
Group: X11/Applications
Source: usermode-1.4.tar.gz
Requires: util-linux
Requires: gtk >= 0.99.970925
Description: Several graphical tools, including a tool to help users manage floppies (and other removable media) and a tool to help the user change his or her finger information.

%changelog

* Fri Nov  7 1997 Otto Hammersmith <otto@redhat.com>

new version that fixed memory leak bug.

* Mon Nov  3 1997 Otto Hammersmith <otto@redhat.com>

updated version to fix bugs

* Fri Oct 17 1997 Otto Hammersmith <otto@redhat.com>

Wrote man pages for userpasswd and userhelper.

* Tue Oct 14 1997 Otto Hammersmith <otto@redhat.com>

Updated the packages... now includes userpasswd for changing passwords
and newer versions of usermount and userinfo.  No known bugs or
misfeatures. 

Fixed the file list...

* Mon Oct 6 1997 Otto Hammersmith <otto@redhat.com>

Created the spec file.

%prep
%setup

%build
make

%install
make install
make install-man

%files
/usr/bin/usermount
/usr/man/man1/usermount.1
/usr/bin/userinfo
/usr/man/man1/userinfo.1
%attr(4755, root, root) /usr/sbin/userhelper
/usr/man/man8/userhelper.8
/usr/bin/userpasswd
/usr/man/man1/userpasswd.1
/etc/X11/wmconfig/userpasswd
/etc/X11/wmconfig/userinfo
/etc/X11/wmconfig/usermount





