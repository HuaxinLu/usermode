Summary: Graphical tools for certain user account management tasks.
Name: usermode
Version: 1.9.1
Release: 1
Copyright: GPL
Group: Applications/System
Source: usermode-%{PACKAGE_VERSION}.tar.gz
Requires: util-linux pam >= 0.66-5 SysVinit >= 2.74-14
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

# Stuff from pam_console, for sysvinit. Here for lack of a better
# place....
mkdir -p $RPM_BUILD_ROOT/etc/pam.d $RPM_BUILD_ROOT/etc/security/console.apps
for wrapapp in shutdown halt reboot poweroff ; do
  ln -sf consolehelper $RPM_BUILD_ROOT/usr/bin/$wrapapp
  touch $RPM_BUILD_ROOT/etc/security/console.apps/$wrapapp
  cp shutdown.pamd $RPM_BUILD_ROOT/etc/pam.d/$wrapapp
done
      
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
/usr/bin/consolehelper
/usr/man/man8/consolehelper.8
/etc/X11/applnk/System/*
# PAM console wrappers
/usr/bin/shutdown
/usr/bin/halt
/usr/bin/reboot
/usr/bin/poweroff
%config(noreplace) /etc/pam.d/shutdown
%config(noreplace) /etc/pam.d/halt
%config(noreplace) /etc/pam.d/reboot
%config(noreplace) /etc/pam.d/poweroff
%config(missingok) /etc/security/console.apps/shutdown
%config(missingok) /etc/security/console.apps/halt
%config(missingok) /etc/security/console.apps/reboot
%config(missingok) /etc/security/console.apps/poweroff


%changelog
* Tue Jul 06 1999 Bill Nottingham <notting@redhat.com>
- import pam_console wrappers from SysVinit, since they require usermode

* Mon Apr 12 1999 Michael K. Johnson <johnsonm@redhat.com>
- even better check for X availability

* Wed Apr 07 1999 Michael K. Johnson <johnsonm@redhat.com>
- better check for X availability
- center windows to make authentication easier (improve later with
  transients and embedded windows where possible)
- applink -> applnk
- added a little padding, especially important when running without
  a window manager, as happens when running from session manager at
  logout time

* Wed Mar 31 1999 Michael K. Johnson <johnsonm@redhat.com>
- hm, need to be root...

* Fri Mar 19 1999 Michael K. Johnson <johnsonm@redhat.com>
- updated userhelper.8 man page for consolehelper capabilities
- moved from wmconfig to desktop entries

* Thu Mar 18 1999 Michael K. Johnson <johnsonm@redhat.com>
- added consolehelper
- Changed conversation architecture to follow PAM spec

* Wed Mar 17 1999 Bill Nottingham <notting@redhat.com>
- remove gdk_input_remove (causing segfaults)

* Tue Jan 12 1999 Michael K. Johnson <johnsonm@redhat.com>
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

