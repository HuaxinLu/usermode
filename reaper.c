/*
 * Copyright (C) 2002 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ident "$Id$"
#include "config.h"
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include "marshal.h"
#include "reaper.h"

#ifdef HAVE_LOCALE_H
#include <locale.h>
#else
#define bindtextdomain(package,dir)
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) dgettext(PACKAGE, String)
#else
#define _(String) String
#endif

static VteReaper *singleton_reaper = NULL;
struct reaper_info {
	int signum;
	pid_t pid;
	int status;
};

static void
vte_reaper_signal_handler(int signum)
{
	struct reaper_info info;
	int status;

	/* This might become more general-purpose in the future, but for now
	 * just forget about signals other than SIGCHLD. */
	info.signum = signum;
	if (signum != SIGCHLD) {
		return;
	}

	if ((singleton_reaper != NULL) && (singleton_reaper->iopipe[0] != -1)) {
		info.pid = waitpid(-1, &status, WNOHANG);
		if (info.pid != -1) {
			info.status = status;
			if (write(singleton_reaper->iopipe[1], "", 0) == 0) {
				write(singleton_reaper->iopipe[1],
				      &info, sizeof(info));
			}
		}
	}
}

static gboolean
vte_reaper_emit_signal(GIOChannel *channel, GIOCondition condition,
		       gpointer data)
{
	struct reaper_info info;
	if (condition != G_IO_IN) {
		return FALSE;
	}
	g_assert(data == singleton_reaper);
	read(singleton_reaper->iopipe[0], &info, sizeof(info));
	if (info.signum == SIGCHLD) {
		g_signal_emit_by_name(data, "child-exited",
				      info.pid, info.status);
	}
	return TRUE;
}

static void
vte_reaper_channel_destroyed(gpointer data)
{
	g_assert_not_reached();
}

static void
vte_reaper_init(VteReaper *reaper, gpointer *klass)
{
	struct sigaction action, old_action;
	int ret;
	ret = pipe(reaper->iopipe);
	if (ret == -1) {
		g_error(_("Error creating signal pipe."));
	}
	action.sa_handler = vte_reaper_signal_handler;
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
	sigaction(SIGCHLD, &action, &old_action);
	reaper->channel = g_io_channel_unix_new(reaper->iopipe[0]);
	g_io_add_watch_full(reaper->channel,
			    G_PRIORITY_HIGH,
			    G_IO_IN,
			    vte_reaper_emit_signal,
			    reaper,
			    vte_reaper_channel_destroyed);
}

static void
vte_reaper_class_init(VteReaperClass *klass, gpointer data)
{
	bindtextdomain(PACKAGE, LOCALEDIR);
	klass->child_exited_signal = g_signal_new("child-exited",
						  G_OBJECT_CLASS_TYPE(klass),
						  G_SIGNAL_RUN_LAST,
						  0,
						  NULL,
						  NULL,
						  _vte_marshal_VOID__INT_INT,
						  G_TYPE_NONE,
						  2, G_TYPE_INT, G_TYPE_INT);
}

GType
vte_reaper_get_type(void)
{
	static GType reaper_type = 0;
	static GTypeInfo reaper_type_info = {
		sizeof(VteReaperClass),

		(GBaseInitFunc)NULL,
		(GBaseFinalizeFunc)NULL,

		(GClassInitFunc)vte_reaper_class_init,
		(GClassFinalizeFunc)NULL,
		NULL,

		sizeof(VteReaper),
		0,
		(GInstanceInitFunc) vte_reaper_init,

		(const GTypeValueTable *) NULL,
	};
	if (reaper_type == 0) {
		reaper_type = g_type_register_static(G_TYPE_OBJECT,
						     "VteReaper",
						     &reaper_type_info,
						     0);
	}
	return reaper_type;
}

VteReaper *
vte_reaper_get(void)
{
	if (!VTE_IS_REAPER(singleton_reaper)) {
		singleton_reaper = g_object_new(VTE_TYPE_REAPER, NULL);
	}
	return singleton_reaper;
}
