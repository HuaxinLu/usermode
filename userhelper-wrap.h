/*
 * Copyright (C) 1997-2001 Red Hat, Inc.
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

#ifndef __USERHELPER_WRAP_H__
#define __USERHELPER_WRAP_H__

/* lots 'o includes. */
#include "userdialogs.h"
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>

#include "userhelper.h"
#define UH_ACTION_AREA "userhelper-action-area"

typedef struct message {
	int type;
	char *message;
	char *data;
	GtkWidget *entry;
	GtkWidget *label;
} message;

struct response {
	int responses, left, rows;
	gboolean ready, fallback_allowed;
	char *user, *service, *suggestion, *banner;;
	GList *message_list; /* contains pointers to messages */
	GtkWidget *dialog, *first, *last, *table;
};

void userhelper_run(char *path, ...);
void userhelper_runv(char *path, const char **args);
void userhelper_fatal_error(int ignored);
void userhelper_sigchld(int signum);	/* sigchld handler */

#endif /* __USERHELPER_WRAP_H__ */
