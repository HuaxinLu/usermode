%define build6x 0
Summary: Graphical tools for certain user account management tasks.
Name: usermode
Version: 1.33
Release: 1
Copyright: GPL
Group: Applications/System
Source: usermode-%{PACKAGE_VERSION}.tar.bz2
%if %{build6x}
Requires: util-linux, pam >= 0.66-5
%else
Requires: util-linux, pam >= 0.66-5, /etc/pam.d/system-auth
%endif
Conflicts: SysVinit < 2.74-14
BuildRoot: %{_tmppath}/usermode-root

%description
The usermode package contains several graphical tools for users:
userinfo, usermount and userpasswd.  Userinfo allows users to change
their finger information.  Usermount lets users mount, unmount, and
format filesystems.  Userpasswd allows users to change their
passwords.

Install the usermode package if you would like to provide users with
graphical tools for certain account management tasks.

%prep
%setup -q

%build
make

%install
rm -rf $RPM_BUILD_ROOT
make PREFIX=$RPM_BUILD_ROOT \
	bindir=%{_bindir} \
	mandir=%{_mandir} \
	sbindir=%{_sbindir} \
	datadir=%{_datadir} \
	install install-man install-po

# Stuff from pam_console, for sysvinit. Here for lack of a better
# place....
mkdir -p $RPM_BUILD_ROOT/etc/pam.d $RPM_BUILD_ROOT/etc/security/console.apps
for wrapapp in halt reboot poweroff ; do
  ln -sf consolehelper $RPM_BUILD_ROOT/usr/bin/$wrapapp
  touch $RPM_BUILD_ROOT/etc/security/console.apps/$wrapapp
%if %{build6x}
  cp shutdown.pamd.6x $RPM_BUILD_ROOT/etc/pam.d/$wrapapp
%else
  cp shutdown.pamd $RPM_BUILD_ROOT/etc/pam.d/$wrapapp
%endif
done

# Strip it!
strip $RPM_BUILD_ROOT%{_bindir}/* $RPM_BUILD_ROOT%{_sbindir}/*
      
%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/usermount
%{_mandir}/man1/usermount.1*
%{_bindir}/userinfo
%{_mandir}/man1/userinfo.1*
%attr(4755,root,root) /usr/sbin/userhelper
%{_mandir}/man8/userhelper.8*
%{_bindir}/userpasswd
%{_mandir}/man1/userpasswd.1*
%{_bindir}/consolehelper
%{_mandir}/man8/consolehelper.8*
%config /etc/X11/applnk/System/*
# PAM console wrappers
%{_bindir}/halt
%{_bindir}/reboot
%{_bindir}/poweroff
%{_datadir}/pixmaps/*
%{_datadir}/locale/*/*/*
%config(noreplace) /etc/pam.d/halt
%config(noreplace) /etc/pam.d/reboot
%config(noreplace) /etc/pam.d/poweroff
%config(missingok) /etc/security/console.apps/halt
%config(missingok) /etc/security/console.apps/reboot
%config(missingok) /etc/security/console.apps/poweroff

%changelog
* Wed Aug 23 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix stdin/stdout redirection shenanigans (#11706)
- fix authentication and execution as users other than root

* Mon Aug 21 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix typo (#16664)

* Sun Aug 20 2000 Nalin Dahyabhai <nalin@redhat.com>
- previous fix, part two

* Sat Aug 19 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix inadvertent breakage of the shell-changing code

* Fri Aug 18 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix the "run unprivileged" option

* Mon Aug 14 2000 Nalin Dahyabhai <nalin@redhat.com>
- actually use the right set of translations

* Fri Aug 11 2000 Nalin Dahyabhai <nalin@redhat.com>
- remove the shutdown command from the list of honored commands

* Wed Aug  9 2000 Nalin Dahyabhai <nalin@redhat.com>
- merge in updated translations
- set XAUTHORITY after successful authentication (#11006)

* Wed Aug  2 2000 Nalin Dahyabhai <nalin@redhat.com>
- install translations
- fixup a messy text string
- make "Mount"/"Unmount" translatable
- stop prompting for passwords to shut down -- we can hit ctrl-alt-del anyway,
  and gdm users can just shut down without logging in

* Mon Jul 31 2000 Nalin Dahyabhai <nalin@redhat.com>
- attempt to add i18n support

* Wed Jul 12 2000 Nalin Dahyabhai <nalin@redhat.com>
- attempt to get a usable icon for userhelper-wrap (#13616, #13768)

* Wed Jul  5 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix them right this time

* Mon Jul  3 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix verbosity problems

* Mon Jun 19 2000 Nalin Dahyabhai <nalin@redhat.com>
- strip all binaries by default
- add the name of the program being run to the userhelper dialog
- add a graphic to the userhelper-wrap package
- add a button to jump straight to nonprivileged operation when supported

* Sun Jun 18 2000 Matt Wilson <msw@redhat.com>
- rebuilt to see if we get stripped binaries

* Mon Jun  5 2000 Nalin Dahyabhai <nalin@redhat.com>
- move man pages to %%{_mandir}

* Thu Jun  1 2000 Nalin Dahyabhai <nalin@redhat.com>
- modify PAM setup to use system-auth
- bzip2 compress tarball

* Fri Mar 17 2000 Ngo Than <than@redhat.de>
- fix problem with LANG and LC_ALL
- compress source with bzip2

* Thu Mar 09 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix problem parsing userhelper's -w flag with other args

* Wed Mar 08 2000 Nalin Dahyabhai <nalin@redhat.com>
- ignore read() == 0 because the child exits

* Tue Mar 07 2000 Nalin Dahyabhai <nalin@redhat.com>
- queue notice messages until we get prompts in userhelper to fix bug #8745

* Fri Feb 03 2000 Nalin Dahyabhai <nalin@redhat.com>
- free trip through the build system

* Tue Jan 11 2000 Nalin Dahyabhai <nalin@redhat.com>
- grab keyboard input focus for dialogs

* Fri Jan 07 2000 Michael K. Johnson <johnsonm@redhat.com>
- The root exploit fix created a bug that only showed up in certain
  circumstances.  Unfortunately, we didn't test in those circumstances...

* Mon Jan 03 2000 Michael K. Johnson <johnsonm@redhat.com>
- fixed local root exploit

* Thu Sep 30 1999 Michael K. Johnson <johnsonm@redhat.com>
- fixed old complex broken gecos parsing, replaced with simple working parsing
- can now blank fields (was broken by previous fix for something else...)

* Tue Sep 21 1999 Michael K. Johnson <johnsonm@redhat.com>
- FALLBACK/RETRY in consolehelper/userhelper
- session management fixed for consolehelper/userhelper SESSION=true
- fix memory leak and failure to close in error condition (#3614)
- fix various bugs where not all elements in userinfo got set

* Mon Sep 20 1999 Michael K. Johnson <johnsonm@redhat.com>
- set $HOME when acting as consolehelper
- rebuild against new pwdb

* Tue Sep 14 1999 Michael K. Johnson <johnsonm@redhat.com>
- honor "owner" flag to mount
- ask for passwords with username

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
