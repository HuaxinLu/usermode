Summary: Graphical tools for certain user account management tasks.
Name: usermode
Version: 1.5
Release: 1
Copyright: GPL
Group: Applications/System
Source: usermode-%{PACKAGE_VERSION}.tar.gz
Requires: util-linux
BuildRoot: /var/tmp/usermode-root

%description
The usermode package contains several graphical tools for users:
userinfo, usermount and userpasswd.  Userinfo allows users to change
their finger information.  Usermount lets users mount, unmount, and
format filesystems.  Userpasswd allows users to change their passwords.

Install the usermode package if you would like to provide users with
graphical tools for certain account management tasks.

%prep
%setup

%build
make

%install
rm -rf $RPM_BUILD_ROOT
make PREFIX=$RPM_BUILD_ROOT install
make PREFIX=$RPM_BUILD_ROOT install-man

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
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

%changelog
* Thu Mar 18 1999 Michael Johnson <johnsonm@redhat.com>
- Changed conversation architecture to follow PAM spec

* Wed Mar 17 1999 Bill Nottingham <notting@redhat.com>
- remove gdk_input_remove (causing segfaults)

* Tue Jan 12 1999 Michael Johnson <johnsonm@redhat.com>
- fix missing include files

* Mon Oct 12 1998 Cristian Gafton <gafton@redhat.com>
- strip binaries
- use defattr
- fix spec file ( rm -rf $(RPM_BUILD_ROOT) is a stupid thing to do ! )

* Tue Oct 06 1998 Preston Brown <pbrown@redhat.com>
- fixed so that the close button on window managers quits the program properly

* Thu Apr 16 1998 Erik Troan <ewt@redhat.com>
- use gtk-config during build
- added make archive rule to Makefile
- uses a build root

* Fri Nov  7 1997 Otto Hammersmith <otto@redhat.com>
- new version that fixed memory leak bug.

* Mon Nov  3 1997 Otto Hammersmith <otto@redhat.com>
- updated version to fix bugs

* Fri Oct 17 1997 Otto Hammersmith <otto@redhat.com>
- Wrote man pages for userpasswd and userhelper.

* Tue Oct 14 1997 Otto Hammersmith <otto@redhat.com>
- Updated the packages... now includes userpasswd for changing passwords
  and newer versions of usermount and userinfo.  No known bugs or
  misfeatures. 
- Fixed the file list...

* Mon Oct 6 1997 Otto Hammersmith <otto@redhat.com>
- Created the spec file.

