/* -*-Mode: c-*- */
/* Copyright (C) 1997 Red Hat Software, Inc.
 * Copyright (C) 2001 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtk/gtk.h>
#include "userdialogs.h"
#include "userhelper-wrap.h"

int
main(int argc, char* argv[])
{
  bindtextdomain("usermode", "/usr/share/locale");
  textdomain("usermode");

  gtk_set_locale();
  gtk_init(&argc, &argv);

  signal(SIGCHLD, userhelper_sigchld);
  userhelper_run(UH_PATH, UH_PATH, UH_PASSWD_OPT, 0);

  gtk_main();

  return 0;
}

void
userhelper_fatal_error(int ignored)
{
  gtk_main_quit();
}
