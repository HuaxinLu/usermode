/*
 * Copyright (C) 1997-2001 Red Hat, Inc.  All rights reserved.
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
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <ctype.h>
#include <glib.h>
#include <grp.h>
#include <libintl.h>
#include <locale.h>
#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include <pwdb/pwdb_public.h>

#include "shvar.h"
#include "userhelper.h"

/* A maximum GECOS field length.  There's no hard limit, so we guess. */
#define GECOS_LENGTH		80

static char *full_name = NULL;		/* full user name */
static char *office = NULL;		/* office */
static char *office_phone = NULL;	/* office phone */
static char *home_phone = NULL;		/* home phone */
static char *user_name = NULL;		/* the account name */
static char *shell_path = NULL;		/* shell path */

/* we manipulate the environment directly, so we have to declare (but not
 * define) the right variable here */
extern char **environ;

/* command line flags */
static int f_flag = 0;		/* -f flag = change full name */
static int o_flag = 0;		/* -o flag = change office name */
static int p_flag = 0;		/* -p flag = change office phone */
static int h_flag = 0;		/* -h flag = change home phone number */
static int c_flag = 0;		/* -c flag = change password */
static int s_flag = 0;		/* -s flag = change shell */
static int t_flag = 0;		/* -t flag = direct text-mode -- exec'ed */
static int w_flag = 0;		/* -w flag = act as a wrapper for next
				 * args */

static struct app_data {
	pam_handle_t *pamh;
	gboolean fallback_allowed, fallback_chosen, cancelled;
	FILE *input, *output;
} app_data = {
	NULL,
	FALSE, FALSE, FALSE,
	NULL, NULL,
};


static int
fail_exit(int retval)
{
	/* This is a local error.  Bail. */
	if (retval == ERR_SHELL_INVALID)
		exit(ERR_SHELL_INVALID);

	if (retval != PAM_SUCCESS) {
		/* Map the PAM error code to a local error code and return
		 * it to the parent process. */
#ifdef DEBUG_USERHELPER
		g_print(_("Got PAM error %d.\n"), retval);
#endif
		switch (retval) {
			case PAM_AUTH_ERR:
			case PAM_PERM_DENIED:
				exit(ERR_PASSWD_INVALID);
			case PAM_AUTHTOK_LOCK_BUSY:
				exit(ERR_LOCKS);
			case PAM_CRED_INSUFFICIENT:
			case PAM_AUTHINFO_UNAVAIL:
				exit(ERR_NO_RIGHTS);
			case PAM_ABORT:
			default:
				exit(ERR_UNK_ERROR);
		}
	}
	/* Just exit. */
	exit(0);
}

/* Read a string from stdin, and return a freshly-allocated copy, without
 * the end-of-line terminator if there was one, and with an optional
 * consolehelper message header removed. */
static char *
read_string(FILE *fp)
{
	char buffer[BUFSIZ], *check = NULL;
	int slen = 0;

	memset(buffer, '\0', sizeof(buffer));

	check = fgets(buffer, sizeof(buffer), fp);
	if(check == NULL) {
		return NULL;
	}

	slen = strlen(buffer);
	if(slen > 0) {
		if(buffer[slen - 1] == '\n') {
			buffer[slen - 1] = '\0';
		}
		if(isspace(buffer[slen - 1])) {
			buffer[slen - 1] = '\0';
		}
	}

	if(buffer[0] == UH_TEXT) {
		memmove(buffer, buffer + 1, sizeof(buffer) - 1);
	}

	return g_strdup(buffer);
}

static int
converse(int num_msg, const struct pam_message **msg,
	 struct pam_response **resp, void *appdata_ptr)
{
	int count = 0;
	int responses = 0;
	struct pam_response *reply = NULL;
	char *noecho_message, *user, *service;
	struct app_data *app_data = appdata_ptr;

	/* Pass on any hints we have to the consolehelper. */
	fprintf(app_data->output, "%d %d\n",
		UH_FALLBACK_ALLOW, app_data->fallback_allowed ? 1 : 0);
	if(pam_get_item(app_data->pamh, PAM_USER,
			(const void**)&user) == PAM_SUCCESS) {
		fprintf(app_data->output, "%d %s\n", UH_USER, user);
	} else {
		user = "root";
	}
	if(pam_get_item(app_data->pamh, PAM_SERVICE,
			(const void**)&service) == PAM_SUCCESS) {
		fprintf(app_data->output, "%d %s\n", UH_SERVICE_NAME, service);
	}

	/* We do first a pass on all items and output them, and then a second
	 * pass to read responses from the helper. */
	for(count = responses = 0; count < num_msg; count++)
	switch(msg[count]->msg_style) {
		case PAM_PROMPT_ECHO_ON:
			/* Spit out the prompt. */
			fprintf(app_data->output, "%d %s\n", UH_ECHO_ON_PROMPT,
				msg[count]->msg);
			responses++;
			break;
		case PAM_PROMPT_ECHO_OFF:
			/* If the prompt is for the user's password, indicate
			 * the user's name if we can.  Otherwise, just output
			 * the prompt as-is. */
			if((strncasecmp(msg[count]->msg, "password", 8) == 0)) {
				noecho_message =
					g_strdup_printf(_("Password for %s"),
							user);
			} else {
				noecho_message = g_strdup(msg[count]->msg);
			}
			fprintf(app_data->output, "%d %s\n",
				UH_ECHO_OFF_PROMPT, noecho_message);
			g_free(noecho_message);
			responses++;
			break;
		case PAM_TEXT_INFO:
			/* Text information strings are output verbatim. */
			fprintf(app_data->output, "%d %s\n",
				UH_INFO_MSG, msg[count]->msg);
			break;
		case PAM_ERROR_MSG:
			/* Error message strings are output verbatim. */
			fprintf(app_data->output, "%d %s\n",
				UH_ERROR_MSG, msg[count]->msg);
			break;
		default:
			/* Maybe the consolehelper can figure out what to do
			 * with this, because we sure can't. */
			fprintf(app_data->output, "%d %s\n",
				UH_UNKNOWN_PROMPT, msg[count]->msg);
	}

	/* Tell the consolehelper how many messages we expect to get
	 * responses to. */
	fprintf(app_data->output, "%d %d\n", UH_EXPECT_RESP, responses);
	fflush(NULL);

	/* Now, for the second pass, allocate space for the responses and read
	 * the answers back. */
	reply = g_malloc0(num_msg * sizeof(struct pam_response));
	app_data->fallback_chosen = FALSE;
	for (count = 0; count < num_msg; count++)
	switch (msg[count]->msg_style) {
		case PAM_TEXT_INFO:
			/* ignore it... */
			reply[count].resp_retcode = PAM_SUCCESS;
			break;
		case PAM_ERROR_MSG:
			/* Also ignore it... */
			reply[count].resp_retcode = PAM_SUCCESS;
			break;
		case PAM_PROMPT_ECHO_ON:
			/* fall through */
		case PAM_PROMPT_ECHO_OFF:
			reply[count].resp = read_string(app_data->input);
			if(reply[count].resp == NULL) {
				/* EOF: the child isn't going to give us any
				 * more information. */
				app_data->cancelled = TRUE;
				g_free(reply);
				return PAM_ABORT;
			}
#ifdef DEBUG_USERHELPER
			if(!isprint(reply[count].resp[0])) {
				fprintf(stderr, "userhelper: got %d\n",
					reply[count].resp[0]);
				fprintf(stderr, "userhelper: got `%s'\n",
					reply[count].resp + 1);
			} else {
				fprintf(stderr, "userhelper: got `%s'\n",
					reply[count].resp);
			}
#endif
			/* If the user chose to abort, do so. */
			if(reply[count].resp[0] == UH_ABORT) {
				app_data->cancelled = TRUE;
				g_free(reply);
				return PAM_ABORT;
			}

			/* If the user chose to fallback, do so. */
			if(reply[count].resp[0] == UH_FALLBACK) {
				app_data->fallback_chosen = TRUE;
				g_free(reply);
				return PAM_ABORT;
			}

			reply[count].resp_retcode = PAM_SUCCESS;
			break;
		default:
			/*
			 * Must be an error of some sort... 
			 */
			g_free(reply);
			return PAM_CONV_ERR;
	}
	*resp = reply;
	return PAM_SUCCESS;
}

/*
 * the structure pointing at the conversation function for
 * auth and changing the password
 */
static struct pam_conv pipe_conv = {
	converse,
	&app_data,
};
static struct pam_conv text_conv = {
	misc_conv,
	&app_data,
};

/* Parse the passed-in GECOS string and set the globals to its broken-down
 * contents.  Note that the string *is* modified here, and the parsing is
 * performed using the convention obeyed by finger(1).  */
static void
parse_gecos(char *gecos)
{
	char *idx;

	if (gecos == NULL)
		return;

	/* The user's full name comes first. */
	if (!full_name)
		full_name = gecos;
	idx = strchr(gecos, ',');
	if (idx != NULL) {
		*idx = '\0';
		gecos = idx + 1;
	}
	if ((idx == NULL) || (*gecos == '\0')) {
		/* no more fields */
		return;
	}

	/* If we have data left, assume the user's office number is next. */
	if (!office)
		office = gecos;
	idx = strchr(gecos, ',');
	if (idx != NULL) {
		*idx = '\0';
		gecos = idx + 1;
	}
	if ((idx == NULL) || (*gecos == '\0')) {
		/* no more fields */
		return;
	}

	/* If we have data left, assume the user's office telephone is next. */
	if (!office_phone)
		office_phone = gecos;
	idx = strchr(gecos, ',');
	if (idx != NULL) {
		*idx = '\0';
		gecos = idx + 1;
	}
	if ((idx == NULL) || (*gecos == '\0')) {
		/* no more fields */
		return;
	}

	/* If we have data left, assume the user's home telephone is next. */
	if (!home_phone)
		home_phone = gecos;
	idx = strchr(gecos, ',');
	if (idx != NULL) {
		*idx = '\0';
	}
}

/* A simple function to compute the size of a gecos string containing the
 * data we have. */
static int
gecos_size(void)
{
	int len = 0;

	if (full_name != NULL)
		len += (strlen(full_name) + 1);
	if (office != NULL)
		len += (strlen(office) + 1);
	if (office_phone != NULL)
		len += (strlen(office_phone) + 1);
	if (home_phone != NULL)
		len += (strlen(home_phone) + 1);
	return len;
}

static gboolean
shell_valid(char *shell_name)
{
	gboolean found;
	char *shell;

	found = FALSE;
	if((shell_name != NULL) && (strlen(shell_name) > 0)) {
		setusershell();
		for(shell = getusershell();
		    shell != NULL;
		    shell = getusershell()) {
#ifdef DEBUG_USERHELPER
			fprintf(stderr, "got shell \"%s\"\n", shell);
#endif
			if (shell_name) {
				if (strcmp(shell_name, shell) == 0) {
					found = TRUE;
					break;
				}
			}
		}
		endusershell();
	}
	return found;
}

static void
become_super(void)
{
	/* Become the superuser. */
	setgroups(0, NULL);
	setregid(0, 0);
	setreuid(0, 0);
	/* Yes, setuid() and friends can fail, even for superusers. */
	if((geteuid() != 0) ||
	   (getuid() != 0) ||
	   (getegid() != 0) ||
	   (getgid() != 0)) {
		exit(ERR_EXEC_FAILED);
	}
}

static void
become_normal(char *user)
{
	/* Join the groups of the user who invoked us. */
	initgroups(user, getgid());
	/* Verify that we're back to normal. */
	if (getegid() != getgid()) {
		exit(ERR_EXEC_FAILED);
	}
	/* Become the user who invoked us. */
	setreuid(getuid(), getuid());
	/* Yes, setuid() can fail. */
	if (geteuid() != getuid()) {
		exit(ERR_EXEC_FAILED);
	}
}

/*
 * ------- the application itself -------- 
 */
int
main(int argc, char **argv)
{
	int arg;
	int retval;
	char *progname = NULL;
	struct passwd *pw;
	struct pam_conv *conv;

	setlocale(LC_ALL, "");
	bindtextdomain("usermode", "/usr/share/locale");
	textdomain("usermode");

	if(geteuid() != 0) {
		fprintf(stderr, _("userhelper must be setuid root\n"));
		exit(ERR_NO_RIGHTS);
	}

	while((w_flag == 0) &&
	      (arg = getopt(argc, argv, "f:o:p:h:s:ctw:")) != -1) {
		/* We process no arguments after -w progname; those are passed
		 * on to a wrapped program. */
		switch (arg) {
			case 'f':
				/* Full name. */
				f_flag++;
				full_name = optarg;
				break;
			case 'o':
				/* Office. */
				o_flag++;
				office = optarg;
				break;
			case 'h':
				/* Home phone. */
				h_flag++;
				home_phone = optarg;
				break;
			case 'p':
				/* Office phone. */
				p_flag++;
				office_phone = optarg;
				break;
			case 's':
				/* Change shell flag. */
				s_flag++;
				shell_path = optarg;
				break;
			case 'c':
				/* Change password flag. */
				c_flag++;
				break;
			case 't':
				/* Text-mode flag. */
				t_flag++;
				break;
			case 'w':
				/* Wrap flag. */
				w_flag++;
				progname = optarg;
				break;
			default:
				exit(ERR_INVALID_CALL);
		}
	}

	/* Sanity-check the arguments a bit. */
#   define SHELL_FLAGS (f_flag || o_flag || h_flag || p_flag || s_flag)
	if((c_flag && SHELL_FLAGS) ||
	   (c_flag && w_flag) ||
	   (w_flag && SHELL_FLAGS)) {
		exit(ERR_INVALID_CALL);
	}

	/* Determine which conversation function to use. */
	if(t_flag > 0) {
		conv = &text_conv;
	} else {
		app_data.input = fdopen(UH_INFILENO, "r");
		app_data.output = fdopen(UH_OUTFILENO, "w");
		if((app_data.input == NULL) || (app_data.output == NULL)) {
			exit(ERR_INVALID_CALL);
		}
		conv = &pipe_conv;
	}

	/* Now try to figure out who called us. */
	pw = getpwuid(getuid());
	if((pw != NULL) && (pw->pw_name != NULL)) {
		user_name = g_strdup(pw->pw_name);
	} else {
		/* I have no name and I must have one. */
		exit(ERR_UNK_ERROR);
	}
#ifdef DEBUG_USERHELPER
	fprintf(stderr, "user is %s\n", user_name);
#endif

	/* If we didn't get the -w flag, the last argument could be a user's
	 * name. */
	if(w_flag == 0) {
		if((getuid() == 0) && (argc == optind + 1)) {
			/* We were started by the superuser, so accept the
			 * user's name. */
			user_name = g_strdup(argv[optind]);
		}

		/* Verify that the user exists. */
		pw = getpwnam(user_name);
		if ((pw == NULL) || (pw->pw_name == NULL)) {
			exit(ERR_NO_USER);
		}
	}

	/* Time to do the heavy lifting. */
	if(c_flag) {
		/* We're here to change the user's password.  Start up PAM 
		 * and tell it we're the "passwd" command. */
		retval = pam_start("passwd", user_name, conv, &app_data.pamh);
		if (retval != PAM_SUCCESS) {
			fail_exit(retval);
		}

		/* Now try to change the user's password. */
		retval = pam_chauthtok(app_data.pamh, 0);
		if (retval != PAM_SUCCESS) {
			fail_exit(retval);
		}

		/* All done! */
		retval = pam_end(app_data.pamh, PAM_SUCCESS);
		if (retval != PAM_SUCCESS) {
			fail_exit(retval);
		}
		exit(0);
	}

	if(SHELL_FLAGS) {
		/* We're here to change the user's GECOS information.  PAM
		 * doesn't provide an interface to do this, because it's not
		 * PAM's job to manage this stuff. */
		char *new_gecos = NULL, *auth_user;
		pwdb_type user_unix[2] = { PWDB_UNIX, _PWDB_MAX_TYPES };
		const struct pwdb *_pwdb = NULL;
		const struct pwdb_entry *_pwe = NULL;
		int tryagain = 3;

		/* Verify that the fields we were given on the command-line
		 * are sane (i.e., contain no forbidden characters). */
		if (f_flag && strpbrk(full_name, ":,="))
			exit(ERR_FIELDS_INVALID);
		if (o_flag && strpbrk(office, ":,="))
			exit(ERR_FIELDS_INVALID);
		if (p_flag && strpbrk(office_phone, ":,="))
			exit(ERR_FIELDS_INVALID);
		if (h_flag && strpbrk(home_phone, ":,="))
			exit(ERR_FIELDS_INVALID);

		/* Start up PAM to authenticate the user, this time pretending
		 * we're "chfn". */
		retval = pam_start("chfn", user_name, conv, &app_data.pamh);
		if (retval != PAM_SUCCESS) {
			fail_exit(retval);
		}

		/* Try to authenticate the user. */
		do {
#ifdef DEBUG_USERHELPER
			fprintf(stderr, _("about to authenticate \"%s\"\n"),
				user_name);
#endif
			retval = pam_authenticate(app_data.pamh, 0);
#ifdef DEBUG_USERHELPER
			fprintf(stderr, _("PAM retval = %d (%s)\n"), retval,
				pam_strerror(app_data.pamh, retval));
#endif
			tryagain--;
		} while((retval != PAM_SUCCESS) && tryagain);

		if (retval != PAM_SUCCESS) {
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}

		/* Verify that the authenticated user is the user we started
		 * out trying to authenticate. */
		retval = pam_get_item(app_data.pamh, PAM_USER,
				      (const void**)&auth_user);
		if(retval != PAM_SUCCESS) {
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}
		if(strcmp(user_name, auth_user) != 0) {
			exit(ERR_UNK_ERROR);
		}

		/* Check if the user is allowed to change her information at
		 * this time, on this machine, yadda, yadda, yadda.... */
		retval = pam_acct_mgmt(app_data.pamh, 0);
		if (retval != PAM_SUCCESS) {
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}

		/* Let's get to it.  Start up PWDB and search for the user. */
		pwdb_start();
		retval = pwdb_locate("user", user_unix, user_name,
				     PWDB_ID_UNKNOWN, &_pwdb);
		if(retval != PWDB_SUCCESS) {
			pam_end(app_data.pamh, PAM_ABORT);
			fail_exit(PAM_ABORT);
		}

		/* Pull up the user's GECOS data, and split it up. */
		retval = pwdb_get_entry(_pwdb, "gecos", &_pwe);
		if(retval == PWDB_SUCCESS) {
			parse_gecos(g_strdup(_pwe->value));
		}

		/* Verify that the strings we got passed are not too long. */
		if(gecos_size() > GECOS_LENGTH) {
			pwdb_entry_delete(&_pwe);
			pwdb_delete(&_pwdb);
			pwdb_end();
			pam_end(app_data.pamh, PAM_ABORT);
			exit(ERR_FIELDS_INVALID);
		}

		/* Build a new value for the GECOS data. */
		new_gecos = g_strdup_printf("%s,%s,%s,%s",
					    full_name ?: "",
					    office ?: "",
					    office_phone ?: "",
					    home_phone ?: "");

		/* We don't need the pwdb_entry for the user's current
		 * GECOS anymore, so free it. */
		pwdb_entry_delete(&_pwe);

		/* Set the user's new GECOS data. */
		retval = pwdb_set_entry(_pwdb, "gecos", new_gecos,
					1 + strlen(new_gecos), NULL, NULL, 0);
		if (retval != PWDB_SUCCESS) {
			pwdb_delete(&_pwdb);
			pwdb_end();
			pam_end(app_data.pamh, PAM_ABORT);
			fail_exit(PAM_ABORT);
		}

		/* If we're here to change the user's shell, too, do that. */
		if(s_flag != 0) {
			/* Check that the user's current shell is valid, and
			 * that she is not attempting to change to an invalid
			 * shell. */
			retval = pwdb_get_entry(_pwdb, "shell", &_pwe);
#ifdef DEBUG_USERHELPER
			fprintf(stderr, "current shell \"%s\"\n",
				(char *) _pwe->value);
			fprintf(stderr, "new shell \"%s\"\n", shell_path);
#endif
			if (!shell_valid(shell_path) ||
			    !shell_valid((char *) _pwe->value) ||
			    retval != PWDB_SUCCESS) {
				fail_exit(ERR_SHELL_INVALID);
				pwdb_entry_delete(&_pwe);
			}
			pwdb_entry_delete(&_pwe);

			/* Set the shell to the new value. */
			retval = pwdb_set_entry(_pwdb, "shell", shell_path,
						1 + strlen(shell_path), NULL,
						NULL, 0);
			if (retval != PWDB_SUCCESS) {
				pwdb_delete(&_pwdb);
				pwdb_end();
				pam_end(app_data.pamh, PAM_ABORT);
				fail_exit(PAM_ABORT);
			}
		}

		/* Save the changes to the user's account to the password
		 * database, whereever that is. */
		retval = pwdb_replace("user", _pwdb->source, user_name,
				      PWDB_ID_UNKNOWN, &_pwdb);
		if (retval != PWDB_SUCCESS) {
			pwdb_delete(&_pwdb);
			pwdb_end();
			pam_end(app_data.pamh, PAM_ABORT);
			fail_exit(PAM_ABORT);
		}
		pwdb_delete(&_pwdb);
		pwdb_end();
	}

	if(w_flag) {
		/* We're here to wrap the named program.  After authenticating
		 * as the user given in the console.apps configuration file,
		 * execute the command given in the console.apps file. */
		char *constructed_path;
		char *apps_filename;
		char *user, *apps_user, *auth_user;
		char *retry, *noxoption;
		char *env_home, *env_term, *env_display, *env_shell;
		char *env_user, *env_logname, *env_lang, *env_lcall;
		char *env_lcmsgs, *env_xauthority;
		int session, tryagain, gui;
		struct stat sbuf;
		shvarFile *s;

		/* Find the basename of the command we're wrapping. */
		if(strrchr(progname, '/')) {
			progname = strrchr(progname, '/') + 1;
		}

		/* Save some of the current environment variables, because the
		 * environment is going to be nuked shortly. */
		env_display = getenv("DISPLAY");
		env_home = getenv("HOME");
		env_lang = getenv("LANG");
		env_lcall = getenv("LC_ALL");
		env_lcmsgs = getenv("LC_MESSAGES");
		env_logname = getenv("LOGNAME");
		env_shell = getenv("SHELL");
		env_term = getenv("TERM");
		env_user = getenv("USER");
		env_xauthority = getenv("XAUTHORITY");

		/* Sanity-check the environment variables as best we can: those
		 * which aren't path names shouldn't contain "/", and none of
		 * them should contain ".." or "%". */
		if(env_display &&
		   (strstr(env_display, "..") ||
		    strchr(env_display, '%')))
			env_display = NULL;
		if(env_home &&
		   (strstr(env_home, "..") ||
		    strchr(env_home, '%')))
			env_home = NULL;
		if(env_lang &&
		   (strstr(env_lang, "/") ||
		    strstr(env_lang, "..") ||
		    strchr(env_lang, '%')))
			env_lang = NULL;
		if(env_lcall &&
		   (strstr(env_lcall, "/") ||
		    strstr(env_lcall, "..") ||
		    strchr(env_lcall, '%')))
			env_lcall = NULL;
		if(env_lcmsgs &&
		   (strstr(env_lcmsgs, "/") ||
		    strstr(env_lcmsgs, "..") ||
		    strchr(env_lcmsgs, '%')))
			env_lcmsgs = NULL;
		if(env_logname &&
		   (strstr(env_logname, "..") ||
		    strchr(env_logname, '%')))
			env_logname = NULL;
		if (env_shell &&
		   (strstr(env_shell, "..") ||
		    strchr(env_shell, '%')))
			env_shell = NULL;
		if (env_term &&
		   (strstr(env_term, "..") ||
		    strchr(env_term, '%')))
			env_term = "dumb";
		if(env_user &&
		   (strstr(env_user, "..") ||
		    strchr(env_user, '%')))
			env_user = NULL;
		if(env_xauthority &&
		   (strstr(env_xauthority , "..") ||
		    strchr(env_xauthority , '%')))
			env_xauthority = NULL;

		/* Wipe out the current environment. */
		environ = g_malloc0(2 * sizeof(char *));

		/* Set just the environment variables we can trust.  Note that
		 * XAUTHORITY is not initially copied -- we don't want to let
		 * attackers get at others' X authority records -- we restore
		 * XAUTHORITY below *after* successfully authenticating, or
		 * abandoning authentication in order to run the wrapped program
		 * as the invoking user. */
		if(env_display) setenv("DISPLAY", env_display, 1);
		if(env_home) {
			setenv("HOME", env_home, 1);
		} else {
			pw = getpwuid(getuid());
			if((pw != NULL) && (pw->pw_dir != NULL)) {
				setenv("HOME", g_strdup(pw->pw_dir), 1);
			}
		}
		if(env_lang) setenv("LANG", env_lang, 1);
		if(env_lcall) setenv("LC_ALL", env_lcall, 1);
		if(env_lcmsgs) setenv("LC_MESSAGES", env_lcmsgs, 1);
		if(env_logname) setenv("LOGNAME", env_logname, 1);
		if(env_shell) setenv("SHELL", env_shell, 1);
		if(env_term) setenv("TERM", env_term, 1);
		if(env_user) setenv("USER", env_user, 1);

		/* Set the PATH to a reasonaly safe list of directories. */
		setenv("PATH",
		       "/usr/sbin:/usr/bin:/sbin:/bin:/usr/X11R6/bin:/root/bin",
		       1);

		/* Open the console.apps configuration file for this wrapped
		 * program, and read settings from it. */
		apps_filename = g_strdup_printf(SYSCONFDIR
						"/security/console.apps/%s",
						progname);
		s = svNewFile(apps_filename);

		/* If the file is world-writable, or isn't a regular file, or
		 * couldn't be open, just exit.  We don't want to alert an
		 * attacker that the service name is invalid. */
		if((fstat(s->fd, &sbuf) == -1) ||
		   !S_ISREG(sbuf.st_mode) ||
		   (sbuf.st_mode & S_IWOTH)) {
			exit(ERR_UNK_ERROR);
		}

		/* Determine who we should authenticate as.  If not specified,
		 * or if "<user>" is specified, we authenticate as the invoking
		 * user, otherwise we authenticate as the specified user (which
		 * is usually root, but could conceivably be something else). */
		apps_user = svGetValue(s, "USER");
		if((apps_user == NULL) || (strcmp(apps_user, "<user>") == 0)) {
			user = user_name;
		} else {
			user = g_strdup(apps_user);
		}

		/* Read the path to the program to run. */
		constructed_path = svGetValue(s, "PROGRAM");
		if(!constructed_path || constructed_path[0] != '/') {
			/* Criminy....  The system administrator didn't give
			 * us an absolute path to the program!  Guess either
			 * /usr/sbin or /sbin, and then give up if there's
			 * nothing in either of those directories. */
			constructed_path = g_strdup_printf("/usr/sbin/%s",
							   progname);
			if(access(constructed_path, X_OK) != 0) {
				/* Try the second directory. */
				strcpy(constructed_path, "/sbin/");
				strcat(constructed_path, progname);
				if(access(constructed_path, X_OK)) {
					/* Nope, not there, either. */
					exit(ERR_NO_PROGRAM);
				}
			}
		}

		/* We can forcefully disable the GUI from the configuration
		 * file (a la blah-nox applications). */
		gui = svTrueValue(s, "GUI", TRUE);
		if(!gui) {
			conv = &text_conv;
		}

		/* We can use for a magic configuration file option to disable
		 * the GUI, too. */
		if(gui) {
			noxoption = svGetValue(s, "NOXOPTION");
			if(noxoption && (strlen(noxoption) > 1)) {
				int i;
				for(i = optind; i < argc; i++) {
					if(strcmp(argv[i], noxoption) == 0) {
						conv = &text_conv;
						break;
					}
				}
			}
		}

		/* Read other settings. */
		session = svTrueValue(s, "SESSION", FALSE);
		app_data.fallback_allowed = svTrueValue(s, "FALLBACK", FALSE);
		retry = svGetValue(s, "RETRY"); /* default value is "2" */
		tryagain = retry ? atoi(retry) + 1 : 3;

		/* Now we're done reading the file. */
		svCloseFile(s);

		/* Start up PAM to authenticate the specified user. */
		retval = pam_start(progname, user, conv, &app_data.pamh);
		if(retval != PAM_SUCCESS) {
			fail_exit(retval);
		}

		/* Try to authenticate the user. */
		do {
#ifdef DEBUG_USERHELPER
			fprintf(stderr, _("about to authenticate \"%s\"\n"),
				user);
#endif
			retval = pam_authenticate(app_data.pamh, 0);
#ifdef DEBUG_USERHELPER
			fprintf(stderr, _("PAM retval = %d (%s)\n"), retval,
				pam_strerror(app_data.pamh, retval));
#endif
			tryagain--;
		} while((retval != PAM_SUCCESS) && tryagain &&
			!app_data.fallback_chosen && !app_data.cancelled);

		if(retval != PAM_SUCCESS) {
			pam_end(app_data.pamh, retval);
			if(app_data.fallback_allowed) {
				/* Reset the user's XAUTHORITY so that the
				 * application can open windows. */
				if(env_xauthority) {
					setenv("XAUTHORITY", env_xauthority, 1);
				}
				argv[optind - 1] = progname;
				become_normal(user_name);
				execv(constructed_path, argv + optind - 1);
				exit(ERR_EXEC_FAILED);
			} else {
				/* Well, we tried. */
				fail_exit(retval);
			}
		}

		/* Verify that the authenticated user is the user we started
		 * out trying to authenticate. */
		retval = pam_get_item(app_data.pamh, PAM_USER,
				      (const void **)&auth_user);
		if(retval != PAM_SUCCESS) {
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}
		if(strcmp(user, auth_user) != 0) {
			exit(ERR_UNK_ERROR);
		}

		/* Verify that the authenticated user is allowed to run this
		 * service now. */
		retval = pam_acct_mgmt(app_data.pamh, 0);
		if (retval != PAM_SUCCESS) {
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}

		/* What we do now depends on whether or not we need to open
		 * a session for the user. */
		if(session) {
			int child, status;

			/* We're opening a session, and that may included
			 * running graphical apps, so restore the XAUTHORITY
			 * environment variable. */
			if(env_xauthority) {
				setenv("XAUTHORITY", env_xauthority, 1);
			}

			/* Open a session. */
			retval = pam_open_session(app_data.pamh, 0);
			if (retval != PAM_SUCCESS) {
				pam_end(app_data.pamh, retval);
				fail_exit(retval);
			}

			/* Start up a child process we can wait on. */
			child = fork();
			if(child == -1) {
				exit(ERR_EXEC_FAILED);
			}
			if(child == 0) {
				/* We're in the child.  Make a few last-minute
				 * preparations and exec the program. */
				char **env_pam;

				env_pam = pam_getenvlist(app_data.pamh);
				while (env_pam && *env_pam) {
#ifdef DEBUG_USERHELPER
					g_print("setting %s\n", *env_pam);
#endif
					putenv(g_strdup(*env_pam));
					env_pam++;
				}

				argv[optind - 1] = progname;
#ifdef DEBUG_USERHELPER
				g_print(_("about to exec \"%s\"\n"),
					constructed_path);
#endif
				become_super();
				execv(constructed_path, argv + optind - 1);
				exit(ERR_EXEC_FAILED);
			}
			/* We're in the parent.  Wait for the child to exit. */
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);

			wait4(child, &status, 0, NULL);

			/* Close the session. */
			retval = pam_close_session(app_data.pamh, 0);
			if (retval != PAM_SUCCESS) {
				pam_end(app_data.pamh, retval);
				fail_exit(retval);
			}

			/* Use the exit status fo the child to determine our
			 * exit value. */
			if (WIFEXITED(status)
			    && (WEXITSTATUS(status) == 0)) {
				pam_end(app_data.pamh, PAM_SUCCESS);
				retval = 1;
			} else {
				pam_end(app_data.pamh, PAM_SUCCESS);
				retval = ERR_EXEC_FAILED;
			}
			exit(retval);
		} else {
			/* We're not opening a session, so we can just exec()
			 * the program we're wrapping. */
			pam_end(app_data.pamh, PAM_SUCCESS);

			argv[optind - 1] = progname;
#ifdef DEBUG_USERHELPER
			g_print(_("about to exec \"%s\"\n"),
				constructed_path);
#endif
			become_super();
			execv(constructed_path, argv + optind - 1);
			exit(ERR_EXEC_FAILED);
		}
	}

	/* Not reached. */
	exit(0);
}
