/*
 * Copyright (C) 1997-2002 Red Hat, Inc.  All rights reserved.
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
#include <glib-object.h>
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

#include <libuser/user.h>

#include "shvar.h"
#include "userhelper.h"

/* A maximum GECOS field length.  There's no hard limit, so we guess. */
#define GECOS_LENGTH			127

static char *full_name = NULL;		/* full user name */
static char *office = NULL;		/* office */
static char *office_phone = NULL;	/* office phone */
static char *home_phone = NULL;		/* home phone */
static char *site_info = NULL;		/* other stuff */
static char *user_name = NULL;		/* the account name */
static char *shell_path = NULL;		/* shell path */

/* we manipulate the environment directly, so we have to declare (but not
 * define) the right variable here */
extern char **environ;
static char **environ_save;

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

/* A structure type which we use to carry psuedo-global data around with us. */
static struct app_data {
	pam_handle_t *pamh;
	gboolean fallback_allowed, fallback_chosen, canceled;
	FILE *input, *output;
	char *banner, *domain;
} app_data = {
	NULL,
	FALSE, FALSE, FALSE,
	NULL, NULL,
	NULL, NULL,
};

/* Exit, returning the proper status code based on a PAM error code. */
static int
fail_exit(int retval)
{
	/* This is a local error.  Bail. */
	if (retval == ERR_SHELL_INVALID) {
		exit(ERR_SHELL_INVALID);
	}

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
				break;
			case PAM_AUTHTOK_LOCK_BUSY:
				exit(ERR_LOCKS);
				break;
			case PAM_CRED_INSUFFICIENT:
			case PAM_AUTHINFO_UNAVAIL:
				exit(ERR_NO_RIGHTS);
				break;
			case PAM_ABORT:
				if (app_data.canceled) {
					_exit(ERR_CANCELED);
				}
				/* fall through */
			default:
				_exit(ERR_UNK_ERROR);
		}
	}
	/* Just exit. */
	_exit(0);
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

	if (feof(fp)) {
		return NULL;
	}
	check = fgets(buffer, sizeof(buffer), fp);
	if (check == NULL) {
		return NULL;
	}

	slen = strlen(buffer);
	if (slen > 0) {
		while ((slen > 1) &&
		       ((buffer[slen - 1] == '\n') ||
			(buffer[slen - 1] == '\r'))) {
			buffer[slen - 1] = '\0';
			slen--;
		}
	}

	if (buffer[0] == UH_TEXT) {
		memmove(buffer, buffer + 1, sizeof(buffer) - 1);
	}

	return g_strdup(buffer);
}

/* A text-mode conversation function suitable for use when there is no
 * controlling terminal. */
static int
silent_converse(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *appdata_ptr)
{
	return PAM_CONV_ERR;
}

/* A mixed-mode conversation function suitable for use with X. */
static int
converse_pipe(int num_msg, const struct pam_message **msg,
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
	if (pam_get_item(app_data->pamh, PAM_USER,
			(const void**)&user) == PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("Sending user `%s'\n", user);
#endif
		fprintf(app_data->output, "%d %s\n", UH_USER, user);
	} else {
		user = "root";
	}
	if (pam_get_item(app_data->pamh, PAM_SERVICE,
			(const void**)&service) == PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("Sending service `%s'\n", service);
#endif
		fprintf(app_data->output, "%d %s\n", UH_SERVICE_NAME, service);
	}
	if ((app_data->domain != NULL) && (app_data->banner != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("Sending banner `%s'\n", app_data->banner);
#endif
		fprintf(app_data->output, "%d %s\n", UH_BANNER,
			dgettext(app_data->domain, app_data->banner));
	}

	/* We do first a pass on all items and output them, and then a second
	 * pass to read responses from the helper. */
	for (count = responses = 0; count < num_msg; count++)
	switch (msg[count]->msg_style) {
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
			if ((strncasecmp(msg[count]->msg, "password", 8) == 0)) {
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
			if (reply[count].resp == NULL) {
				/* EOF: the child isn't going to give us any
				 * more information. */
				app_data->canceled = TRUE;
				g_free(reply);
				return PAM_ABORT;
			}
#ifdef DEBUG_USERHELPER
			if (!isprint(reply[count].resp[0])) {
				g_print("userhelper: got %d\n",
					reply[count].resp[0]);
				g_print("userhelper: got `%s'\n",
					reply[count].resp[0] ?
					reply[count].resp + 1 : "");
			} else {
				g_print("userhelper: got `%s'\n",
					reply[count].resp[0] ?
					reply[count].resp : "");
			}
#endif
			/* If the user chose to abort, do so. */
			if (reply[count].resp[0] == UH_ABORT) {
				app_data->canceled = TRUE;
				g_free(reply);
				return PAM_ABORT;
			}

			/* If the user chose to fallback, do so. */
			if (reply[count].resp[0] == UH_FALLBACK) {
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

/* A conversation function which wraps the one provided by libpam_misc. */
static int
converse_console(int num_msg, const struct pam_message **msg,
		 struct pam_response **resp, void *appdata_ptr)
{
	static int banner = 0;
	const char *service = NULL, *user = NULL, *codeset = NULL;
	char *text = NULL;
	struct app_data *app_data = appdata_ptr;
	struct pam_message **messages;
	int i, ret;

	g_get_charset(&codeset);
	bind_textdomain_codeset(PACKAGE, codeset);

	pam_get_item(app_data->pamh, PAM_SERVICE, (const void**)&service);
	pam_get_item(app_data->pamh, PAM_USER, (const void**)&user);

	if (banner == 0) {
		if ((app_data->banner != NULL) && (app_data->domain != NULL)) {
			text = g_strdup_printf(dgettext(app_data->domain, app_data->banner));
		} else {
			if ((service != NULL) && (strlen(service) > 0)) {
				if (app_data->fallback_allowed) {
					text = g_strdup_printf(_("You are attempting to run \"%s\" which may benefit from administrative\nprivileges, but more information is needed in order to do so."), service);
				} else {
					text = g_strdup_printf(_("You are attempting to run \"%s\" which requires administrative\nprivileges, but more information is needed in order to do so."), service);
				}
			} else {
				if (app_data->fallback_allowed) {
					text = g_strdup_printf(_("You are attempting to run a command which may benefit from\nadministrative privileges, but more information is needed in order to do so."));
				} else {
					text = g_strdup_printf(_("You are attempting to run a command which requires administrative\nprivileges, but more information is needed in order to do so."));
				}
			}
		}
		if (text != NULL) {
			fprintf(stdout, "%s\n", text);
			fflush(stdout);
			g_free(text);
		}
		banner++;
	}

	messages = g_malloc0(sizeof(struct pam_message) * (num_msg + 1));
	for (i = 0; i < num_msg; i++) {
		messages[i] = g_malloc(sizeof(struct pam_message));
		*(messages[i]) = *(msg[i]);
		if (msg[i]->msg != NULL) {
			if ((strncasecmp(msg[i]->msg, "password", 8) == 0)) {
				messages[i]->msg =
					g_strdup_printf(_("Password for %s: "),
							user);
			} else {
				messages[i]->msg = g_strdup(_(msg[i]->msg));
			}
		}
	}

	ret = misc_conv(num_msg, (const struct pam_message**)messages,
			resp, appdata_ptr);

	for (i = 0; i < num_msg; i++) {
		if (messages[i]->msg != NULL) {
			g_free((char*)messages[i]->msg);
		}
		g_free(messages[i]);
	}
	g_free(messages);

	return ret;
}

/* A mixed-mode libuser prompter callback. */
static gboolean
prompt_pipe(struct lu_prompt *prompts, int prompts_count,
	    gpointer callback_data, struct lu_error **error)
{
	int i;
	char *user, *service;
	struct app_data *app_data = callback_data;

	/* Pass on any hints we have to the consolehelper. */
	fprintf(app_data->output, "%d %d\n",
		UH_FALLBACK_ALLOW, app_data->fallback_allowed ? 1 : 0);
	if (pam_get_item(app_data->pamh, PAM_USER,
			(const void**)&user) == PAM_SUCCESS) {
		fprintf(app_data->output, "%d %s\n", UH_USER, user);
	} else {
		user = "root";
	}
	if (pam_get_item(app_data->pamh, PAM_SERVICE,
			(const void**)&service) == PAM_SUCCESS) {
		fprintf(app_data->output, "%d %s\n", UH_SERVICE_NAME, service);
	}

	/* We do first a pass on all items and output them, and then a second
	 * pass to read responses from the helper. */
	for (i = 0; i < prompts_count; i++) {
		/* Spit out the prompt. */
		if (prompts[i].default_value) {
			fprintf(app_data->output, "%d %s\n",
				UH_PROMPT_SUGGESTION,
				prompts[i].default_value);
		}
		fprintf(app_data->output, "%d %s\n",
			prompts[i].visible ?
			UH_ECHO_ON_PROMPT :
			UH_ECHO_OFF_PROMPT,
			prompts[i].prompt);
	}

	/* Tell the consolehelper how many messages we expect to get
	 * responses to. */
	fprintf(app_data->output, "%d %d\n", UH_EXPECT_RESP, prompts_count);
	fflush(NULL);

	/* Now, for the second pass, allocate space for the responses and read
	 * the answers back. */
	for (i = 0; i < prompts_count; i++) {
		prompts[i].value = read_string(app_data->input);
		if (prompts[i].value == NULL) {
			/* EOF: the child isn't going to give us any more
			 * information. */
			app_data->canceled = TRUE;
			return PAM_ABORT;
		}
		prompts[i].free_value = g_free;
#ifdef DEBUG_USERHELPER
		if (!isprint(prompts[i].value[0])) {
			g_print("userhelper: got %d\n", prompts[i].value[0]);
			g_print("userhelper: got `%s'\n",
				prompts[i].value[0] ?
				prompts[i].value + 1 : "");
		} else {
			g_print("userhelper: got `%s'\n", prompts[i].value);
		}
#endif
		/* If the user chose to abort, do so. */
		if (prompts[i].value[0] == UH_ABORT) {
			app_data->canceled = TRUE;
			return FALSE;
		}

		/* If the user chose to fallback, do so. */
		if (prompts[i].value[0] == UH_FALLBACK) {
			app_data->fallback_chosen = TRUE;
			return FALSE;
		}
	}
	return TRUE;
}

/* PAM conversation structures containing the addresses of the various
 * conversation functions and our global data. */
static struct pam_conv silent_conv = {
	silent_converse,
	&app_data,
};
static struct pam_conv pipe_conv = {
	converse_pipe,
	&app_data,
};
static struct pam_conv text_conv = {
	converse_console,
	&app_data,
};

/* Parse the passed-in GECOS string and set the globals to its broken-down
 * contents.  Note that the string *is* modified here, and the parsing is
 * performed using the convention obeyed by BSDish finger(1) under Linux.  */
static void
parse_gecos(char *gecos)
{
	char **exploded, **dest;
	int i;

	if (gecos == NULL) {
		return;
	}
	exploded = g_strsplit(gecos, ",", 5);

	if (exploded == NULL) {
		return;
	}

	for (i = 0; (exploded != NULL) && (exploded[i] != NULL); i++) {
		dest = NULL;
		switch (i) {
			case 0:
				dest = &full_name;
				break;
			case 1:
				dest = &office;
				break;
			case 2:
				dest = &office_phone;
				break;
			case 3:
				dest = &home_phone;
				break;
			case 4:
				dest = &site_info;
				break;
			default:
				g_assert_not_reached();
				break;
		}
		if (*dest != NULL) {
			*dest = g_strdup(exploded[i]);
		}
	}

	g_strfreev(exploded);
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
	if (site_info != NULL)
		len += (strlen(site_info) + 1);
	return len;
}

static gboolean
shell_valid(char *shell_name)
{
	gboolean found;
	char *shell;

	found = FALSE;
	if ((shell_name != NULL) && (strlen(shell_name) > 0)) {
		setusershell();
		for (shell = getusershell();
		     shell != NULL;
		     shell = getusershell()) {
#ifdef DEBUG_USERHELPER
			g_print("got shell \"%s\"\n", shell);
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
	if ((geteuid() != 0) ||
	    (getuid() != 0) ||
	    (getegid() != 0) ||
	    (getgid() != 0)) {
#ifdef DEBUG_USERHELPER
		g_print("set*id() failure: %s\n", strerror(errno));
#endif
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
#ifdef DEBUG_USERHELPER
		g_print("still setgid()\n");
#endif
		exit(ERR_EXEC_FAILED);
	}
	/* Become the user who invoked us. */
	setreuid(getuid(), getuid());
	/* Yes, setuid() can fail. */
	if (geteuid() != getuid()) {
#ifdef DEBUG_USERHELPER
		g_print("still setuid()\n");
#endif
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
	gboolean ret;
	char *progname = NULL;
	struct passwd *pw;
	struct pam_conv *conv;
	lu_prompt_fn *prompt;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, DATADIR "/locale");
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	if (geteuid() != 0) {
		fprintf(stderr, _("userhelper must be setuid root\n"));
#ifdef DEBUG_USERHELPER
		g_print("not setuid\n");
#endif
		exit(ERR_NO_RIGHTS);
	}

	while ((w_flag == 0) &&
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
#ifdef DEBUG_USERHELPER
				g_print("invalid call: unknown option\n");
#endif
				exit(ERR_INVALID_CALL);
		}
	}

	/* Sanity-check the arguments a bit. */
#define SHELL_FLAGS (f_flag || o_flag || h_flag || p_flag || s_flag)
	if ((c_flag && SHELL_FLAGS) ||
	    (c_flag && w_flag) ||
	    (w_flag && SHELL_FLAGS)) {
#ifdef DEBUG_USERHELPER
		g_print("invalid call: invalid combination of options\n");
#endif
		exit(ERR_INVALID_CALL);
	}

	/* Determine which conversation function to use. */
	if (t_flag) {
		/* We're in text mode. */
		if (isatty(STDIN_FILENO)) {
			/* We have a controlling tty which we can disable
			 * echoing on. */
			conv = &text_conv;
		} else {
			/* We have no controlling terminal -- being run from
			 * cron or some other mechanism? */
			conv = &silent_conv;
#if 0
			/* FIXME: print a warning here? */
			fprintf(stderr, _("Unable to open graphical window, "
				"and unable to find controlling terminal.\n"));
			_exit(0);
#endif
		}
		prompt = &lu_prompt_console;
	} else {
		/* Set up to use the GTK+ helper. */
		app_data.input = fdopen(UH_INFILENO, "r");
		app_data.output = fdopen(UH_OUTFILENO, "w");
		if ((app_data.input == NULL) || (app_data.output == NULL)) {
#ifdef DEBUG_USERHELPER
			g_print("invalid call\n");
#endif
			exit(ERR_INVALID_CALL);
		}
		conv = &pipe_conv;
		prompt = &prompt_pipe;
	}

	/* Now try to figure out who called us. */
	pw = getpwuid(getuid());
	if ((pw != NULL) && (pw->pw_name != NULL)) {
		user_name = g_strdup(pw->pw_name);
	} else {
		/* I have no name and I must have one. */
#ifdef DEBUG_USERHELPER
		g_print("i have no name");
#endif
		exit(ERR_UNK_ERROR);
	}
#ifdef DEBUG_USERHELPER
	g_print("user is %s\n", user_name);
#endif

	/* If we didn't get the -w flag, the last argument could be a user's
	 * name. */
	if (w_flag == 0) {
		if ((getuid() == 0) && (argc == optind + 1)) {
			/* We were started by the superuser, so accept the
			 * user's name. */
			user_name = g_strdup(argv[optind]);
		}

		/* Verify that the user exists. */
		pw = getpwnam(user_name);
		if ((pw == NULL) || (pw->pw_name == NULL)) {
#ifdef DEBUG_USERHELPER
			g_print("user %s doesn't exist\n", user_name);
#endif
			exit(ERR_NO_USER);
		}
	}

	/* Time to do the heavy lifting. */
	if (c_flag) {
		int tryagain = 1;
		/* We're here to change the user's password.  Start up PAM 
		 * and tell it we're the "passwd" command. */
		retval = pam_start("passwd", user_name, conv, &app_data.pamh);
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("pam_start() failed\n");
#endif
			fail_exit(retval);
		}

		/* Set the requesting user. */
		retval = pam_set_item(app_data.pamh, PAM_RUSER, user_name);
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("pam_set_item() failed\n");
#endif
			fail_exit(retval);
		}

		/* Now try to change the user's password. */
		do {
#ifdef DEBUG_USERHELPER
			g_print("about to change password for \"%s\"\n",
				user_name);
#endif
			retval = pam_chauthtok(app_data.pamh, 0);
#ifdef DEBUG_USERHELPER
			g_print("PAM retval = %d (%s)\n", retval,
				pam_strerror(app_data.pamh, retval));
#endif
			tryagain--;
		} while ((retval != PAM_SUCCESS) &&
			 (retval != PAM_CONV_ERR) &&
			 !app_data.canceled &&
			 tryagain);
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("pam_chauthtok() failed\n");
#endif
			fail_exit(retval);
		}

		/* All done! */
		retval = pam_end(app_data.pamh, PAM_SUCCESS);
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("pam_end() failed\n");
#endif
			fail_exit(retval);
		}
		exit(0);
	}

	if (SHELL_FLAGS) {
		/* We're here to change the user's non-security information.
		 * PAM doesn't provide an interface to do this, because it's
		 * not PAM's job to manage this stuff, so farm it out to a
		 * different library. */
		char *new_gecos = NULL, *auth_user, *gecos = NULL;
		char *old_shell = NULL;
		struct lu_context *context;
		struct lu_ent *ent = NULL;
		struct lu_error *error = NULL;
		GValueArray *values;
		GValue *value, val;
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
#ifdef DEBUG_USERHELPER
			g_print("pam_start() failed\n");
#endif
			fail_exit(retval);
		}

		/* Set the requesting user. */
		retval = pam_set_item(app_data.pamh, PAM_RUSER, user_name);
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("pam_set_item() failed\n");
#endif
			fail_exit(retval);
		}

		/* Try to authenticate the user. */
		do {
#ifdef DEBUG_USERHELPER
			g_print("about to authenticate \"%s\"\n", user_name);
#endif
			retval = pam_authenticate(app_data.pamh, 0);
#ifdef DEBUG_USERHELPER
			g_print("PAM retval = %d (%s)\n", retval,
				pam_strerror(app_data.pamh, retval));
#endif
			tryagain--;
		} while ((retval != PAM_SUCCESS) &&
			 (retval != PAM_CONV_ERR) &&
			 !app_data.canceled &&
			 tryagain);
		/* If we didn't succeed, bail. */
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("pam authentication failed\n");
#endif
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}

		/* Verify that the authenticated user is the user we started
		 * out trying to authenticate. */
		retval = pam_get_item(app_data.pamh, PAM_USER,
				      (const void**)&auth_user);
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("no pam user set\n");
#endif
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}
		/* At some point this check will go away. */
		if (strcmp(user_name, auth_user) != 0) {
#ifdef DEBUG_USERHELPER
			g_print("username(%s) != authuser(%s)", user_name,
				auth_user);
#endif
			exit(ERR_UNK_ERROR);
		}

		/* Check if the user is allowed to change her information at
		 * this time, on this machine, yadda, yadda, yadda.... */
		retval = pam_acct_mgmt(app_data.pamh, 0);
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("pam_acct_mgmt() failed");
#endif
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}

		/* Let's get to it.  Start up libuser. */
		context = lu_start(user_name, lu_user, NULL, NULL,
				   prompt,
				   (gpointer)&app_data, &error);
		if (context == NULL) {
#ifdef DEBUG_USERHELPER
			g_print("libuser startup error");
#endif
			pam_end(app_data.pamh, PAM_ABORT);
			fail_exit(PAM_ABORT);
		}
		if (error != NULL) {
#ifdef DEBUG_USERHELPER
			g_print("libuser startup error: %s",
				lu_strerror(error));
#endif
			pam_end(app_data.pamh, PAM_ABORT);
			fail_exit(PAM_ABORT);
		}

		/* Look up the user's record. */
		ent = lu_ent_new();
		ret = lu_user_lookup_name(context, user_name, ent, &error);
		if (ret != TRUE) {
#ifdef DEBUG_USERHELPER
			g_print("libuser doesn't know the user");
#endif
			pam_end(app_data.pamh, PAM_ABORT);
			fail_exit(PAM_ABORT);
		}
		if (error != NULL) {
#ifdef DEBUG_USERHELPER
			g_print("libuser startup error: %s",
				lu_strerror(error));
#endif
			pam_end(app_data.pamh, PAM_ABORT);
			fail_exit(PAM_ABORT);
		}

		/* Pull up the user's GECOS data, and split it up. */
		values = lu_ent_get(ent, LU_GECOS);
		if (values != NULL) {
			value = g_value_array_get_nth(values, 0);
			if (G_VALUE_HOLDS_STRING(value)) {
				gecos = g_value_dup_string(value);
			} else
			if (G_VALUE_HOLDS_LONG(value)) {
				gecos = g_strdup_printf("%ld", g_value_get_long(value));
			} else {
				g_assert_not_reached();
			}
			parse_gecos(gecos);
		}

		/* Verify that the strings we got passed are not too long. */
		if (gecos_size() > GECOS_LENGTH) {
#ifdef DEBUG_USERHELPER
			g_print("user gecos too long %d > %d", gecos_size(), GECOS_LENGTH);
#endif
			lu_ent_free(ent);
			lu_end(context);
			pam_end(app_data.pamh, PAM_ABORT);
			exit(ERR_FIELDS_INVALID);
		}

		/* Build a new value for the GECOS data. */
		new_gecos = g_strdup_printf("%s,%s,%s,%s%s%s",
					    full_name ?: "",
					    office ?: "",
					    office_phone ?: "",
					    home_phone ?: "",
					    site_info ? "," : "",
					    site_info ?: "");

		/* We don't need the user's current GECOS anymore, so clear
		 * out the value and set our own in the in-memory structure. */
		memset(&val, 0, sizeof(val));
		g_value_init(&val, G_TYPE_STRING);

		lu_ent_clear(ent, LU_GECOS);
		g_value_set_string(&val, new_gecos);
		lu_ent_add(ent, LU_GECOS, &val);

		/* While we're at it, set the individual data items as well. */
		lu_ent_clear(ent, LU_COMMONNAME);
		g_value_set_string(&val, full_name);
		lu_ent_add(ent, LU_COMMONNAME, &val);

		lu_ent_clear(ent, LU_ROOMNUMBER);
		g_value_set_string(&val, office);
		lu_ent_add(ent, LU_ROOMNUMBER, &val);

		lu_ent_clear(ent, LU_TELEPHONENUMBER);
		g_value_set_string(&val, office_phone);
		lu_ent_add(ent, LU_TELEPHONENUMBER, &val);

		lu_ent_clear(ent, LU_HOMEPHONE);
		g_value_set_string(&val, home_phone);
		lu_ent_add(ent, LU_HOMEPHONE, &val);

		/* If we're here to change the user's shell, too, do that
		 * while we're in here, assuming that chsh and chfn have
		 * idential PAM configurations. */
		if (s_flag != 0) {
			/* Check that the user's current shell is valid, and
			 * that she is not attempting to change to an invalid
			 * shell. */
			values = lu_ent_get(ent, LU_LOGINSHELL);
			if (values != NULL) {
				value = g_value_array_get_nth(values, 0);
				if (G_VALUE_HOLDS_STRING(value)) {
					old_shell= g_value_dup_string(value);
				} else
				if (G_VALUE_HOLDS_LONG(value)) {
					old_shell = g_strdup_printf("%ld", g_value_get_long(value));
				} else {
					g_assert_not_reached();
				}
			}

#ifdef DEBUG_USERHELPER
			g_print("current shell \"%s\"\n", old_shell);
			g_print("new shell \"%s\"\n", shell_path);
#endif
			/* If the old or new shell are invalid, then
			 * the user doesn't get to make the change. */
			if (!shell_valid(shell_path) ||
			    !shell_valid(old_shell)) {
#ifdef DEBUG_USERHELPER
				g_print("bad shell value\n");
#endif
				lu_ent_free(ent);
				lu_end(context);
				pam_end(app_data.pamh, PAM_ABORT);
				fail_exit(ERR_SHELL_INVALID);
			}

			/* Set the shell to the new value. */
			lu_ent_clear(ent, LU_LOGINSHELL);
			g_value_set_string(&val, shell_path);
			lu_ent_add(ent, LU_LOGINSHELL, &val);
		}

		/* Save the changes to the user's account to the password
		 * database, whereever that is. */
		ret = lu_user_modify(context, ent, &error);
		if (ret != TRUE) {
			lu_ent_free(ent);
			lu_end(context);
			pam_end(app_data.pamh, PAM_ABORT);
#ifdef DEBUG_USERHELPER
			g_print("libuser save failed\n");
#endif
			fail_exit(PAM_ABORT);
		}
		if (error != NULL) {
#ifdef DEBUG_USERHELPER
			g_print("libuser save error: %s",
				lu_strerror(error));
#endif
			lu_ent_free(ent);
			lu_end(context);
			pam_end(app_data.pamh, PAM_ABORT);
			fail_exit(PAM_ABORT);
		}

		lu_ent_free(ent);
		lu_end(context);
		_exit(0);
	}

	if (w_flag) {
		/* We're here to wrap the named program.  After authenticating
		 * as the user given in the console.apps configuration file,
		 * execute the command given in the console.apps file. */
		char *constructed_path;
		char *apps_filename;
		char *user_pam = user_name, *apps_user, *auth_user;
		char *apps_banner, *apps_domain;
		char *retry, *noxoption;
		char *env_home, *env_term, *env_desktop_startup_id;
		char *env_display, *env_shell;
		char *env_lang, *env_lcall, *env_lcmsgs, *env_xauthority;
		int session, tryagain, gui;
		struct stat sbuf;
		shvarFile *s;

		/* Find the basename of the command we're wrapping. */
		if (strrchr(progname, '/')) {
			progname = strrchr(progname, '/') + 1;
		}

		/* Save some of the current environment variables, because the
		 * environment is going to be nuked shortly. */
		env_desktop_startup_id = getenv("DESKTOP_STARTUP_ID");
		env_display = getenv("DISPLAY");
		env_home = getenv("HOME");
		env_lang = getenv("LANG");
		env_lcall = getenv("LC_ALL");
		env_lcmsgs = getenv("LC_MESSAGES");
		env_shell = getenv("SHELL");
		env_term = getenv("TERM");
		env_xauthority = getenv("XAUTHORITY");

		/* Sanity-check the environment variables as best we can: those
		 * which aren't path names shouldn't contain "/", and none of
		 * them should contain ".." or "%". */
		if (env_display &&
		    (strstr(env_display, "..") ||
		     strchr(env_display, '%')))
			env_display = NULL;
		if (env_home &&
		    (strstr(env_home, "..") ||
		     strchr(env_home, '%')))
			env_home = NULL;
		if (env_lang &&
		    (strstr(env_lang, "/") ||
		     strstr(env_lang, "..") ||
		     strchr(env_lang, '%')))
			env_lang = NULL;
		if (env_lcall &&
		    (strstr(env_lcall, "/") ||
		     strstr(env_lcall, "..") ||
		     strchr(env_lcall, '%')))
			env_lcall = NULL;
		if (env_lcmsgs &&
		    (strstr(env_lcmsgs, "/") ||
		     strstr(env_lcmsgs, "..") ||
		     strchr(env_lcmsgs, '%')))
			env_lcmsgs = NULL;
		if (env_shell &&
		    (strstr(env_shell, "..") ||
		     strchr(env_shell, '%')))
			env_shell = NULL;
		if (env_term &&
		    (strstr(env_term, "..") ||
		     strchr(env_term, '%')))
			env_term = "dumb";
		if (env_xauthority &&
		    (strstr(env_xauthority , "..") ||
		     strchr(env_xauthority , '%')))
			env_xauthority = NULL;

		/* Wipe out the current environment. */
		environ_save = environ;
		environ = g_malloc0(2 * sizeof(char *));

		/* Set just the environment variables we can trust.  Note that
		 * XAUTHORITY is not initially copied -- we don't want to let
		 * attackers get at others' X authority records -- we restore
		 * XAUTHORITY below *after* successfully authenticating, or
		 * abandoning authentication in order to run the wrapped program
		 * as the invoking user. */
		if (env_display) setenv("DISPLAY", env_display, 1);

		/* The rest of the environment variables are simpler. */
		if (env_desktop_startup_id) setenv("DESKTOP_STARTUP_ID",
						   env_desktop_startup_id, 1);
		if (env_lang) setenv("LANG", env_lang, 1);
		if (env_lcall) setenv("LC_ALL", env_lcall, 1);
		if (env_lcmsgs) setenv("LC_MESSAGES", env_lcmsgs, 1);
		if (env_shell) setenv("SHELL", env_shell, 1);
		if (env_term) setenv("TERM", env_term, 1);

		/* Set the PATH to a reasonaly safe list of directories. */
		setenv("PATH",
		       "/usr/sbin:/usr/bin:/sbin:/bin:/usr/X11R6/bin:/root/bin",
		       1);

		/* Set the LOGNAME and USER variables to the executing name. */
		setenv("LOGNAME", g_strdup("root"), 1);
		setenv("USER", g_strdup("root"), 1);

		/* Open the console.apps configuration file for this wrapped
		 * program, and read settings from it. */
		apps_filename = g_strdup_printf(SYSCONFDIR
						"/security/console.apps/%s",
						progname);
		s = svNewFile(apps_filename);

		/* If the file is world-writable, or isn't a regular file, or
		 * couldn't be open, just exit.  We don't want to alert an
		 * attacker that the service name is invalid. */
		if ((s == NULL) ||
		    (fstat(s->fd, &sbuf) == -1) ||
		    !S_ISREG(sbuf.st_mode) ||
		    (sbuf.st_mode & S_IWOTH)) {
#ifdef DEBUG_USERHELPER
			g_print("bad file permissions\n");
#endif
			exit(ERR_UNK_ERROR);
		}

		/* Determine who we should authenticate as.  If not specified,
		 * or if "<user>" is specified, we authenticate as the invoking
		 * user, otherwise we authenticate as the specified user (which
		 * is usually root, but could conceivably be something else). */
		apps_user = svGetValue(s, "USER");
		if ((apps_user == NULL) || (strcmp(apps_user, "<user>") == 0)) {
			user_pam = user_name;
		} else {
			user_pam = g_strdup(apps_user);
		}

		/* Read the path to the program to run. */
		constructed_path = svGetValue(s, "PROGRAM");
		if (!constructed_path || constructed_path[0] != '/') {
			/* Criminy....  The system administrator didn't give
			 * us an absolute path to the program!  Guess either
			 * /usr/sbin or /sbin, and then give up if there's
			 * nothing in either of those directories. */
			constructed_path = g_strdup_printf("/usr/sbin/%s",
							   progname);
			if (access(constructed_path, X_OK) != 0) {
				/* Try the second directory. */
				strcpy(constructed_path, "/sbin/");
				strcat(constructed_path, progname);
				if (access(constructed_path, X_OK)) {
					/* Nope, not there, either. */
#ifdef DEBUG_USERHELPER
					g_print("couldn't find binary\n");
#endif
					exit(ERR_NO_PROGRAM);
				}
			}
		}

		/* We can forcefully disable the GUI from the configuration
		 * file (a la blah-nox applications). */
		gui = svTrueValue(s, "GUI", TRUE);
		if (!gui) {
			conv = &text_conv;
		}

		/* We can use a magic configuration file option to disable
		 * the GUI, too. */
		if (gui) {
			noxoption = svGetValue(s, "NOXOPTION");
			if (noxoption && (strlen(noxoption) > 1)) {
				int i;
				for (i = optind; i < argc; i++) {
					if (strcmp(argv[i], noxoption) == 0) {
						conv = &text_conv;
						break;
					}
				}
			}
		}

		/* Verify that the user we need to authenticate as has a home
		 * directory. */
		pw = getpwnam(user_pam);
		if (pw == NULL) {
#ifdef DEBUG_USERHELPER
			g_print("no user named %s exists\n", user_pam);
#endif
			exit(ERR_NO_USER);
		}

		/* If the user we're authenticating as has root's UID, then it's
		 * safe to let them use HOME=~root. */
		if (pw->pw_uid == 0) {
			setenv("HOME", g_strdup(pw->pw_dir), 1);
		} else {
			/* Otherwise, if they had a reasonable value for HOME,
			 * let them use it. */
			if (env_home) {
				setenv("HOME", env_home, 1);
			} else {
				/* Otherwise, punt. */
				pw = getpwuid(getuid());
				if ((pw != NULL) && (pw->pw_dir != NULL)) {
					setenv("HOME", g_strdup(pw->pw_dir), 1);
				}
			}
		}

		/* Read other settings. */
		session = svTrueValue(s, "SESSION", FALSE);
		app_data.fallback_allowed = svTrueValue(s, "FALLBACK", FALSE);
		retry = svGetValue(s, "RETRY"); /* default value is "2" */
		tryagain = retry ? atoi(retry) + 1 : 3;

		/* Read any custom messages we might want to use. */
		apps_banner = svGetValue(s, "BANNER");
		if ((apps_banner != NULL) && (strlen(apps_banner) > 0)) {
			app_data.banner = apps_banner;

		}
		apps_domain = svGetValue(s, "BANNER_DOMAIN");
		if ((apps_domain != NULL) && (strlen(apps_domain) > 0)) {
			bindtextdomain(apps_domain, DATADIR "/locale");
			bind_textdomain_codeset(apps_domain, "UTF-8");
			app_data.domain = apps_domain;
		}

		/* Now we're done reading the file. */
		svCloseFile(s);

		/* Start up PAM to authenticate the specified user. */
		retval = pam_start(progname, user_pam, conv, &app_data.pamh);
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("pam_start() failed\n");
#endif
			fail_exit(retval);
		}

		/* Set the requesting user. */
		retval = pam_set_item(app_data.pamh, PAM_RUSER, user_name);
		if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
			g_print("pam_set_item() failed\n");
#endif
			fail_exit(retval);
		}

		/* Try to authenticate the user. */
		do {
#ifdef DEBUG_USERHELPER
			g_print("about to authenticate \"%s\"\n", user_pam);
#endif
			retval = pam_authenticate(app_data.pamh, 0);
#ifdef DEBUG_USERHELPER
			g_print("PAM retval = %d (%s)\n", retval,
				pam_strerror(app_data.pamh, retval));
#endif
			tryagain--;
		} while ((retval != PAM_SUCCESS) && tryagain &&
			 !app_data.fallback_chosen && !app_data.canceled);

		if (retval != PAM_SUCCESS) {
			pam_end(app_data.pamh, retval);
			if (app_data.canceled) {
				fail_exit(retval);
			} else if (app_data.fallback_allowed) {
				/* Reset the user's environment so that the
				 * application can run normally. */
				argv[optind - 1] = progname;
				environ = environ_save;
				become_normal(user_name);
				if (app_data.input != NULL) {
				       fclose(app_data.input);
				       close(UH_INFILENO);
				}
				if (app_data.output != NULL) {
				       fclose(app_data.output);
				       close(UH_OUTFILENO);
				}
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
		if (retval != PAM_SUCCESS) {
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}
		if (strcmp(user_pam, auth_user) != 0) {
			exit(ERR_UNK_ERROR);
		}

		/* Verify that the authenticated user is allowed to run this
		 * service now. */
		retval = pam_acct_mgmt(app_data.pamh, 0);
		if (retval != PAM_SUCCESS) {
			pam_end(app_data.pamh, retval);
			fail_exit(retval);
		}

		/* We need to re-read the user's information -- libpam doesn't
		 * guarantee that these won't be nuked. */
		pw = getpwnam(user_pam);
		if (pw == NULL) {
#ifdef DEBUG_USERHELPER
			g_print("no user named %s exists\n", user_pam);
#endif
			exit(ERR_NO_USER);
		}

		/* What we do now depends on whether or not we need to open
		 * a session for the user. */
		if (session) {
			int child, status;

			/* We're opening a session, and that may included
			 * running graphical apps, so restore the XAUTHORITY
			 * environment variable. */
			if (env_xauthority) {
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
			if (child == -1) {
				exit(ERR_EXEC_FAILED);
			}
			if (child == 0) {
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
				if (app_data.input != NULL) {
				       fclose(app_data.input);
				       close(UH_INFILENO);
				}
				if (app_data.output != NULL) {
				       fclose(app_data.output);
				       close(UH_OUTFILENO);
				}
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
			if (WIFEXITED(status)) {
				pam_end(app_data.pamh, PAM_SUCCESS);
				retval = 0;
			} else {
				pam_end(app_data.pamh, PAM_SUCCESS);
				retval = ERR_UNK_ERROR;
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
			if (app_data.input != NULL) {
			       fclose(app_data.input);
			       close(UH_INFILENO);
			}
			if (app_data.output != NULL) {
			       fclose(app_data.output);
			       close(UH_OUTFILENO);
			}
			execv(constructed_path, argv + optind - 1);
			exit(ERR_EXEC_FAILED);
		}
	}

	/* Not reached. */
	exit(0);
}
