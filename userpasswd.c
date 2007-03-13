/* -*-Mode: c-*- */
/* Copyright (C) 1997 Red Hat Software, Inc.
 * Copyright (C) 2001, 2007 Red Hat, Inc.
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

#include "config.h"
#include <libintl.h>
#include <gtk/gtk.h>
#include "userhelper-wrap.h"

void
userhelper_fatal_error(int ignored)
{
	userhelper_main_quit();
}

int
main(int argc, char *argv[])
{
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	gtk_init(&argc, &argv);

	userhelper_run(TRUE, UH_PATH, UH_PATH, UH_PASSWD_OPT, 0);

	return 0;
}
