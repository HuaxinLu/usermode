%define build6x 0
Summary: Tools for certain user account management tasks.
Name: usermode
Version: 1.50
Release: 1
License: GPL
Group: Applications/System
Source: usermode-%{version}.tar.gz
%if %{build6x}
Requires: util-linux, pam >= 0.66-5
%else
Requires: util-linux, pam >= 0.66-5, /etc/pam.d/system-auth
%endif
Conflicts: SysVinit < 2.74-14
BuildPrereq: glib-devel, gtk2-devel, libglade2-devel, pam-devel, util-linux
BuildRoot: %{_tmppath}/%{name}-root

%package gtk
Summary: Graphical tools for certain user account management tasks.
Group: Applications/System
Requires: %{name} = %{version}-%{release}

%description
The usermode package contains the userhelper program, which can be
used to allow configured programs to be run with superuser privileges
by ordinary users.

%description gtk
The usermode-gtk package contains several graphical tools for users:
userinfo, usermount and userpasswd.  Userinfo allows users to change
their finger information.  Usermount lets users mount, unmount, and
format filesystems.  Userpasswd allows users to change their
passwords.

Install the usermode-gtk package if you would like to provide users with
graphical tools for certain account management tasks.

%prep
%setup -q

%build
%configure
make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

# We set up the shutdown programs to be wrapped in this package.  Other
# packages are on their own....
mkdir -p $RPM_BUILD_ROOT/etc/pam.d $RPM_BUILD_ROOT/etc/security/console.apps
for wrappedapp in halt reboot poweroff ; do
	ln -s consolehelper $RPM_BUILD_ROOT%{_bindir}/${wrappedapp}
	touch $RPM_BUILD_ROOT/etc/security/console.apps/${wrappedapp}
%if %{build6x}
	cp shutdown.pamd.6x $RPM_BUILD_ROOT/etc/pam.d/${wrappedapp}
%else
	cp shutdown.pamd $RPM_BUILD_ROOT/etc/pam.d/${wrappedapp}
%endif
done

# Strip it!  Strip it good!
strip $RPM_BUILD_ROOT%{_bindir}/* $RPM_BUILD_ROOT%{_sbindir}/* || :
%find_lang %{name}

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}.lang
%defattr(-,root,root)
%attr(4711,root,root) /usr/sbin/userhelper
%{_bindir}/consolehelper
%{_mandir}/man8/userhelper.8*
%{_mandir}/man8/consolehelper.8*
# PAM console wrappers
%{_bindir}/halt
%{_bindir}/reboot
%{_bindir}/poweroff
%if %{build6x}
%{_bindir}/shutdown
%endif
%config(noreplace) /etc/pam.d/halt
%config(noreplace) /etc/pam.d/reboot
%config(noreplace) /etc/pam.d/poweroff
%config /etc/security/console.apps/halt
%config /etc/security/console.apps/reboot
%config /etc/security/console.apps/poweroff

%files gtk
%defattr(-,root,root)
%config /etc/X11/applnk/System/*
%{_bindir}/usermount
%{_mandir}/man1/usermount.1*
%{_bindir}/userinfo
%{_mandir}/man1/userinfo.1*
%{_bindir}/userpasswd
%{_mandir}/man1/userpasswd.1*
%{_bindir}/consolehelper-gtk
%{_datadir}/pixmaps/*

# If you're updating translations, do me a favor and bump the RELEASE number,
# and not the VERSION number.  Version numbers indicate CODE changes.
%changelog
* Thu Jan 31 2002 Nalin Dahyabhai <nalin@redhat.com> 1.50-1
- fix userpasswd dialog message being incorrect for password changes
- use a dumb conversation function when text mode is invoked without a tty -- if
  the service's configuration doesn't call for prompts, then it'll still work
- port from pwdb to libuser
- catch child-exit errors correctly again
- fix keyboard-grabbing

* Wed Jan 23 2002 Nalin Dahyabhai <nalin@redhat.com> 1.49-3
- add default locations for certain binaries to configure.in

* Thu Jan  3 2002 Nalin Dahyabhai <nalin@redhat.com> 1.49-2
- munge glade file to use stock items for buttons where possible

* Mon Dec 10 2001 Nalin Dahyabhai <nalin@redhat.com> 1.49-1
- the console.apps configs shouldn't be missingok
- fix buildprereqs for gtk2/libglade2

* Tue Dec  4 2001 Nalin Dahyabhai <nalin@redhat.com>
- more gtk2 changes
- split off a -gtk subpackage with all of the gtk-specific functionality

* Wed Nov 28 2001 Nalin Dahyabhai <nalin@redhat.com>
- the grand libglade/gtk2 overhaul
- allow disabling display of GUI windows by setting "GUI=false" in the
  console.apps configuration file (default: TRUE)
- allow disabling display of GUI windows by recognizing a magic option
  on the command-line of the program being wrapped (NOXOPTION, no default)

* Fri Nov  9 2001 Nalin Dahyabhai <nalin@redhat.com> 1.46-1
- restore the previous XAUTHORITY setting before opening PAM sessions

* Fri Nov  2 2001 Nalin Dahyabhai <nalin@redhat.com> 1.45-1
- propagate environment variables from libpam to applications

* Fri Oct  3 2001 Nalin Dahyabhai <nalin@redhat.com> 1.44-1
- only try to call gtk_main_quit() if we've got a loop to get out of (#54109)
- obey RPM_OPT_FLAGS, obey

* Tue Aug 28 2001 Trond Eivind Glomsr�d <teg@redhat.com> 1.43-1
- Update translations

* Mon Aug  6 2001 Nalin Dahyabhai <nalin@redhat.com>
- add build requirements on glib-devel, gtk+-devel, pam-devel (#49726)

* Sun Jun 24 2001 Elliot Lee <sopwith@redhat.com>
- Bump release + rebuild.

* Wed Feb 14 2001 Preston Brown <pbrown@redhat.com>
- final translation merge.

* Wed Feb 14 2001 Nalin Dahyabhai <nalin@redhat.com>
- clear the supplemental groups list before running binaries as root (#26851)

* Wed Feb  7 2001 Nalin Dahyabhai <nalin@redhat.com>
- set XAUTHORITY if we fall back to regular behavior (#26343)
- make the suid helper 04711 instead of 04755

* Mon Feb  5 2001 Nalin Dahyabhai <nalin@redhat.com>
- refresh translations

* Mon Jan 29 2001 Preston Brown <pbrown@redhat.com>
- use lang finding script.

* Thu Jan 25 2001 Yukihiro Nakai <ynakai@redhat.com>
- Some fix for Japanese environment.
- Use gtk_set_locale() instead of setlocale()
- Copyright update.

* Sun Jan  7 2001 Yukihiro Nakai <ynakai@redhat.com>
- Add gettextized

* Thu Nov  2 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix segfault in userhelper (#20027)

* Tue Oct 24 2000 Nalin Dahyabhai <nalin@redhat.com>
- /sbin/shutdown, not /usr/sbin/shutdown (#19034)

* Fri Oct  6 2000 Nalin Dahyabhai <nalin@redhat.com>
- don't pass on arguments to halt and reboot, because they error out

* Thu Oct  5 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix the /usr/bin/shutdown wrapper so that root can call shutdown
- only include the /usr/bin/shutdown wrapper on 6.x
- also sanitize LC_MESSAGES
- tweak sanitizing checks (from mkj)

* Wed Oct  4 2000 Jakub Jelinek <jakub@redhat.com>
- fix a security bug with LC_ALL/LANG variables (#18046)

* Mon Aug 28 2000 Nalin Dahyabhai <nalin@redhat.com>
- mark defined strings translateable (#17006)

* Thu Aug 24 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix incorrect user name
- add a shell wrapper version of /usr/bin/shutdown
- build for 6.x errata
- bump revision to upgrade the errata

* Wed Aug 23 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix stdin/stdout redirection shenanigans (#11706)
- fix authentication and execution as users other than root
- make sure the right descriptors are terminals before dup2()ing them
- cut out an extra-large CPU waster that breaks GUI apps

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
