%if %{?WITH_SELINUX:0}%{!?WITH_SELINUX:1}
%define WITH_SELINUX 1
%endif

%define build6x 0
Summary: Tools for certain user account management tasks.
Name: usermode
Version: 1.69
Release: 4
License: GPL
Group: Applications/System
Source: usermode-%{version}-%{release}.tar.gz
%if %{build6x}
Requires: util-linux, pam >= 0.66-5
%else
Requires: util-linux, pam >= 0.75-37, /etc/pam.d/system-auth
%endif
Conflicts: SysVinit < 2.74-14
BuildPrereq: desktop-file-utils, glib2-devel, gtk2-devel
BuildPrereq: libglade2-devel, libuser-devel, pam-devel, util-linux
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
%setup -q -n %{name}-%{version}-%{release}

%build
%configure \
%if %{WITH_SELINUX}
	--with-selinux 
%endif

make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

# We set up the shutdown programs to be wrapped in this package.  Other
# packages are on their own....
mkdir -p $RPM_BUILD_ROOT/etc/pam.d $RPM_BUILD_ROOT/etc/security/console.apps
for wrappedapp in halt reboot poweroff ; do
	ln -s consolehelper $RPM_BUILD_ROOT%{_bindir}/${wrappedapp}
	install -m644 $wrappedapp $RPM_BUILD_ROOT/etc/security/console.apps/${wrappedapp}
%if %{build6x}
	cp shutdown.pamd.6x $RPM_BUILD_ROOT/etc/pam.d/${wrappedapp}
%else
	cp shutdown.pamd $RPM_BUILD_ROOT/etc/pam.d/${wrappedapp}
%endif
done
%if %{WITH_SELINUX}
install -m644 userhelper_context $RPM_BUILD_ROOT/etc/security
%endif
%if ! %{build6x}
rm -f $RPM_BUILD_ROOT/%{_bindir}/shutdown
%endif

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
%if %{WITH_SELINUX}
%config /etc/security/userhelper_context
%endif

%files gtk
%defattr(-,root,root)
%{_bindir}/usermount
%{_mandir}/man1/usermount.1*
%{_bindir}/userinfo
%{_mandir}/man1/userinfo.1*
%{_bindir}/userpasswd
%{_mandir}/man1/userpasswd.1*
%{_bindir}/consolehelper-gtk
%{_bindir}/pam-panel-icon
%{_datadir}/%{name}
%{_datadir}/pixmaps/*
%{_datadir}/applications/*

%changelog
* Tue Jan 27 2004 Dan Walsh <dwalsh@redhat.com> 1.69-4
- fix call to is_selinux_enabled

* Mon Dec  8 2003 Nalin Dahyabhai <nalin@redhat.com>
- fix warning in userinfo which would cause random early exit (#111409)
- clean up warnings

* Tue Nov 25 2003 Dan Walsh <dwalsh@redhat.com> 1.69-3.sel
- Fix handling of roles from console file

* Fri Nov 14 2003 Nalin Dahyabhai <nalin@redhat.com>
- don't disable use of deprecated GLib and GTK+ APIs, reported by the
  mysterious Pierre-with-no-last-name

* Thu Oct 30 2003 Dan Walsh <dwalsh@redhat.com> 1.69-2.sel
- Turn on sleinux

* Thu Oct 23 2003 Nalin Dahyabhai <nalin@redhat.com> 1.69-1
- all around: cleanups
- consolehelper: coalesce multiple messages from PAM again
- usermount: handle user-not-allowed-to-control-mounts error correctly (#100457)
- userhelper: trim off terminating commas when changing chfn info

* Mon Oct 6 2003 Dan Walsh <dwalsh@redhat.com> 1.68-8

* Wed Oct 1 2003 Dan Walsh <dwalsh@redhat.com> 1.68-7.sel
- Fix to use /etc instead of /usr/etc

* Thu Sep 25 2003 Dan Walsh <dwalsh@redhat.com> 1.68-6.sel
- turn on selinux
- add default userhelper context file

* Thu Sep 25 2003 Nalin Dahyabhai <nalin@redhat.com> 1.68-6
- make selinux a configure option to avoid screwing with makefiles

* Thu Sep 25 2003 Nalin Dahyabhai <nalin@redhat.com> 1.68-5
- rebuild

* Mon Sep 8 2003 Dan Walsh <dwalsh@redhat.com> 1.68-4
- turn off selinux

* Fri Sep 5 2003 Dan Walsh <dwalsh@redhat.com> 1.68-3.sel
- turn on selinux

* Tue Jul 29 2003 Dan Walsh <dwalsh@redhat.com> 1.68-2
- Add SELinux support

* Wed Apr 16 2003 Nalin Dahyabhai <nalin@redhat.com> 1.68-1
- update translations
- suppress the error dialog from user cancel

* Mon Feb 24 2003 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Thu Feb 20 2003 Nalin Dahyabhai <nalin@redhat.com> 1.67-1
- work around GTK+ clearing DESKTOP_STARTUP_ID at gtk_init() time, so that
  startup notification actually works (#84684)

* Wed Feb 19 2003 Nalin Dahyabhai <nalin@redhat.com> 1.66-1
- consolehelper-gtk: complete startup notification at startup
- userhelper: pass startup notification data to consolehelper-gtk
- consolehelper-gtk: setup startup notification for children if userhelper
  requests it

* Mon Jan 27 2003 Nalin Dahyabhai <nalin@redhat.com> 1.65-2
- rebuild

* Mon Jan 20 2003 Nalin Dahyabhai <nalin@redhat.com> 1.65-1
- pass-through DESKTOP_STARTUP_ID

* Mon Jan  6 2003 Nalin Dahyabhai <nalin@redhat.com> 1.64-1
- set the requesting user PAM item to the invoking user's name (#81255)

* Mon Nov 11 2002 Nalin Dahyabhai <nalin@redhat.com> 1.63-2
- remove directory names from PAM config files, allowing the same config
  files to work for both arches on multilib boxes
- translation updates

* Wed Sep  4 2002 Nalin Dahyabhai <nalin@redhat.com> 1.63-1
- userhelper: swallow the exec'd program's exit status, which would be
  misinterpreted by consolehelper anyway

* Tue Sep  3 2002 Nalin Dahyabhai <nalin@redhat.com> 1.62-1
- consolehelper: suppress dialog on successful execution
- userhelper: return 0 on success, not 1 (what was I *thinking*?)

* Mon Sep  2 2002 Nalin Dahyabhai <nalin@redhat.com> 1.61-1
- userinfo: exit properly on escape. handle site_info field properly. go
  insensitive while running child process.
- userpasswd: exit properly on cancel.
- all of the above: reap the child instead of checking for pipe close -- this
  way is more robust (#68578,72684).
- usermount: run mount/umount synchronously. capture stderr and display in a
  dialog. desensitize action buttons when no filesystems are selected.
- consolehelper: display errors if we're attempting to run bogus programs
  (#72127)
- translation updates (#70278)

* Wed Aug 14 2002 Nalin Dahyabhai <nalin@redhat.com> 1.60-1
- reconnect the "cancel" and "ok" buttons in userinfo
- heed the cancel button when prompting for passwords in userinfo (#68578)
- translation update

* Wed Aug 14 2002 Nalin Dahyabhai <nalin@redhat.com> 1.59-2
- change "forget password" to "forget authorization", because we don't actually
  remember the password (that would be scary, #71476)
- translation update

* Tue Aug 13 2002 Nalin Dahyabhai <nalin@redhat.com> 1.59-1
- pam-panel-icon: overhaul, change the 'locked' icon to keyring-small, nix the
  'unlocked' icon
- consolehelper-gtk: properly set up the dialog buttons (should be 'cancel/ok'
  when we're asking questions, was always 'close')
- disappear pam_timestamp_init

* Wed Aug  7 2002 Nalin Dahyabhai <nalin@redhat.com> 1.58-2
- install the new 'unlocked' icon

* Tue Aug  6 2002 Jonathan Blandford <jrb@redhat.com>
- New version.

* Mon Aug  5 2002 Nalin Dahyabhai <nalin@redhat.com> 1.57-1
- add support for BANNER and BANNER_DOMAIN in the userhelper configuration

* Mon Aug  5 2002 Nalin Dahyabhai <nalin@redhat.com> 1.56-4
- mark strings in the .glade file as translatable (#70278)
- translation updates

* Wed Jul 31 2002 Nalin Dahyabhai <nalin@redhat.com> 1.56-3
- add icons for userpasswd and usermount

* Wed Jul 24 2002 Nalin Dahyabhai <nalin@redhat.com> 1.56-2
- actually include the icons
- translation updates

* Tue Jul 23 2002 Nalin Dahyabhai <nalin@redhat.com> 1.56-1
- userinfo: prevent users from selecting "nologin" as a shell (#68579)
- don't strip binaries by default; leave that to the buildroot policy
- use desktop-file-install

* Wed Jun 19 2002 Havoc Pennington <hp@redhat.com>
- put pam-panel-icon in file list

* Mon May 20 2002 Nalin Dahyabhai <nalin@redhat.com> 1.55-2
- don't strip binaries which have no special privileges

* Wed May 15 2002 Nalin Dahyabhai <nalin@redhat.com> 1.55-1
- remove the pixmap we don't use any more (we use stock pixmaps now)
- update translations

* Thu Apr 16 2002 Nalin Dahyabhai <nalin@redhat.com> 1.54-1
- suppress even error messages from Xlib when consolehelper calls
  gtk_init_check() to see if the display is available

* Mon Apr 15 2002 Nalin Dahyabhai <nalin@redhat.com> 1.53-2
- refresh translations

* Thu Apr 11 2002 Nalin Dahyabhai <nalin@redhat.com> 1.53-1
- refresh shell variable code from authconfig (#63175)

* Tue Apr  9 2002 Nalin Dahyabhai <nalin@redhat.com> 1.52-2
- refresh translations

* Mon Apr  1 2002 Nalin Dahyabhai <nalin@redhat.com> 1.52-1
- attempt to make prompts at the console more meaningful
- when falling back, reset the entire environment to the user's

* Thu Mar 28 2002 Nalin Dahyabhai <nalin@redhat.com>
- stop giving the user chances to enter the right password if we get a
  conversation error reading a response (appears to be masked by libpam)
  (#62195)
- always center consolehelper dialog windows

* Wed Mar 27 2002 Nalin Dahyabhai <nalin@redhat.com> 1.51-1
- patch to make gettext give us UTF-8 strings (which GTK needs) from ynakai

* Fri Mar 22 2002 Nalin Dahyabhai <nalin@redhat.com> 1.50-6
- update translations
- actually include the glade files (#61665)

* Mon Mar 11 2002 Nalin Dahyabhai <nalin@redhat.com> 1.50-5
- update translations

* Mon Feb 25 2002 Nalin Dahyabhai <nalin@redhat.com> 1.50-4
- rebuild

* Fri Feb 22 2002 Nalin Dahyabhai <nalin@redhat.com> 1.50-3
- update translations

* Thu Jan 31 2002 Nalin Dahyabhai <nalin@redhat.com> 1.50-2
- rebuild to fix dependencies

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
