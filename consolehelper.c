/* Copyright (C) 1999-2001 Red Hat Software, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <libintl.h>
#include <unistd.h>
#include <sys/types.h>
#include <gtk/gtk.h>
#include "userdialogs.h"
#include "userhelper-wrap.h"

void
userhelper_fatal_error(int signal)
{
  gtk_main_quit();
}

int
main(int argc, char* argv[])
{
  char *display;
  char **constructed_argv;
  int cargc, cdiff;
  char *progname;
  int graphics_available = 0;
  int fake_gtk_argc = 1;
  char **fake_gtk_argv;

  setlocale(LC_ALL, "");
  bindtextdomain("usermode", "/usr/share/locale");
  textdomain("usermode");

  constructed_argv = g_malloc0((argc+4) * sizeof(char *));

  constructed_argv[0] = UH_PATH;
  progname = strrchr(argv[0], '/');
  if (progname) {
    progname++; /* pass the '/' character */
  } else {
    progname = argv[0];
  }

  fake_gtk_argv = g_malloc0((fake_gtk_argc + 1) * sizeof(char *));
  fake_gtk_argv[0] = argv[0];

  display = getenv("DISPLAY");

  if ((((display != NULL) && (strlen(display) > 0)) || !isatty(STDIN_FILENO))) {
    if (gtk_init_check(&fake_gtk_argc, &fake_gtk_argv)) {
      gtk_set_locale();
      graphics_available = 1;
    }
  }

  if (graphics_available) {
    constructed_argv[1] = UH_WRAP_OPT;
    constructed_argv[2] = progname;
    cdiff = 2;
  } else {
    constructed_argv[1] = UH_TEXT_OPT;
    constructed_argv[2] = UH_WRAP_OPT;
    constructed_argv[3] = progname;
    cdiff = 3;
  }

  for (cargc = 1; cargc < argc; constructed_argv[cargc+cdiff] = argv[cargc++]);

  if (graphics_available) {
    signal(SIGCHLD, userhelper_fatal_error);
    userhelper_runv(UH_PATH, constructed_argv);
    gtk_main();
  } else {
    /* text mode doesn't need the whole pipe thing... */
    execv(UH_PATH, constructed_argv);
    return 1;
  }

  return 0;
}
