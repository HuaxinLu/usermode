/* Copyright (C) 1997-1999 Red Hat Software, Inc.
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

#define UH_PATH "/usr/sbin/userhelper"
/* #define UH_PATH "./userhelper" */
#define UH_PASSWD_OPT "-c"
#define UH_FULLNAME_OPT "-f"
#define UH_OFFICE_OPT "-o"
#define UH_OFFICEPHONE_OPT "-p"
#define UH_HOMEPHONE_OPT "-h"
#define UH_SHELL_OPT "-s"
#define UH_TEXT_OPT "-t"
#define UH_WRAP_OPT "-w"


typedef struct message {
	int type;
	char *message;
	char *data;
	GtkWidget *entry;
	GtkWidget *label;
} message;

typedef struct response {
	int num_components;
	GSList *message_list; /* contains pointers to messages */
	GtkWidget *top;
	GtkWidget *table;
	GtkWidget *ok;
	GtkWidget *cancel;
	message *head;
	message *tail;
} response;

void userhelper_run_passwd();
void userhelper_run_chfn(char* fullname, char* office, 
			 char* officephone, char* homephone, 
			 char* shell);
void userhelper_parse_exitstatus(int exitstatus);
void userhelper_parse_childout();
void userhelper_read_childout(gpointer data, int source, GdkInputCondition cond);
void userhelper_write_childin(GtkWidget* widget, response *resp);

void userhelper_sigchld();	/* sigchld handler */

void userhelper_fatal_error();

#endif /* __USERHELPER_WRAP_H__ */
