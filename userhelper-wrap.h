/* -*-Mode: c-*- */
/* Copyright (C) 1997 Red Hat Software, Inc.
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
#include <sys/time.h>
#include <sys/wait.h>

#define UH_PATH "/usr/sbin/userhelper"
/* #define UH_PATH "./userhelper" */
#define UH_PASSWD_OPT "-c"
#define UH_FULLNAME_OPT "-f"
#define UH_OFFICE_OPT "-o"
#define UH_OFFICEPHONE_OPT "-p"
#define UH_HOMEPHONE_OPT "-h"
/* #define UH_SHELL_OPT "" */

/* fix userhelper so these match the pam.h defines. */
#define UH_ECHO_ON_PROMPT 1
#define UH_ECHO_OFF_PROMPT 2
#define UH_INFO_MSG 3
#define UH_ERROR_MSG 4

/* make a common header that userhelper will get these from, as well. */
#define ERR_PASSWD_INVALID      1       /* password is not right */
#define ERR_FIELDS_INVALID      2       /* gecos fields invalid or
                                         * sum(lengths) too big */
#define ERR_SET_PASSWORD        3       /* password resetting error */
#define ERR_LOCKS               4       /* some files are locked */
#define ERR_NO_USER             5       /* user unknown ... */
#define ERR_NO_RIGHTS           6       /* insufficient rights  */
#define ERR_INVALID_CALL        7       /* invalid call to this program */
#define ERR_UNK_ERROR           255     /* unknown error */

void userhelper_run_passwd();
void userhelper_run_chfn(char* fullname, char* office, 
			 char* officephone, char* homephone, 
			 char* shell);
void userhelper_parse_exitstatus(int exitstatus);
void userhelper_parse_childout();
void userhelper_read_childout(gpointer data, int source, GdkInputCondition cond);
/* int userhelper_write_childin(); */
void userhelper_write_childin(GtkWidget* widget, GtkWidget* entry);

void userhelper_sigchld();	/* sigchld handler */

void userhelper_fatal_error();

#endif /* __USERHELPER_WRAP_H__ */
