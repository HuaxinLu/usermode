/*
 * Copyright (C) 1997-2003 Red Hat, Inc.  All rights reserved.
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
#include <fcntl.h>
#include <grp.h>
#include <libintl.h>
#include <locale.h>
#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include <libuser/user.h>

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <selinux/get_context_list.h>
#define CONTEXT_FILE SYSCONFDIR "/security/userhelper_context"
#endif

#include "shvar.h"
#include "userhelper.h"

/* A maximum GECOS field length.  There's no hard limit, so we guess. */
#define GECOS_LENGTH			127

/* A structure to hold broken-out GECOS data.  The number and names of the
 * fields are dictated entirely by the flavor of finger we use.  Seriously. */
struct gecos_data {
	char *full_name;	/* full user name */
	char *office;		/* office */
	char *office_phone;	/* office phone */
	char *home_phone;	/* home phone */
	char *site_info;	/* other stuff */
};

/* We manipulate the environment directly, so we have to declare (but not
 * define) the right variable here. */
extern char **environ;

#ifdef WITH_SELINUX
static gboolean selinux_enabled = FALSE;
static security_context_t old_context = NULL; /* our original securiy context */
static security_context_t new_context = NULL; /* our target security context */
#endif

/* A structure type which we use to carry psuedo-global data around with us. */
static struct app_data {
	pam_handle_t *pamh;
	gboolean fallback_allowed, fallback_chosen, canceled;
	FILE *input, *output;
	char *banner, *domain;
#ifdef USE_STARTUP_NOTIFICATION
	char *sn_name, *sn_description, *sn_wmclass;
	char *sn_binary_name, *sn_icon_name, *sn_id;
	int sn_workspace;
#endif
} app_data = {
	NULL,
	FALSE, FALSE, FALSE,
	NULL, NULL,
	NULL, NULL,
#ifdef USE_STARTUP_NOTIFICATION
	NULL, NULL, NULL,
	NULL, NULL, NULL,
	-1,
#endif
};

#ifdef WITH_SELINUX
static void
setup_selinux_exec(char *constructed_path)
{
	if (selinux_enabled) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: exec \"%s\" with %s context\n",
			constructed_path, new_context);
#endif
		if (setexeccon(new_context) < 0) {
			fprintf(stderr,
				_("Could not set exec context to %s.\n"),
				new_context);
			exit(-1);
		}
		if (new_context) {
			freecon(new_context);
		}
	}
}

/*
 * get_init_context()
 *
 * Get the init CONTEXT for this program 
 *
 * in:		nothing
 * out:		The CONTEXT associated with the context.
 * return:	0 on success, -1 on failure.
 */
static int
get_init_context(const char *context_file, security_context_t *context)
{
	FILE *fp;
	char buf[LINE_MAX], *bufp;
	int buf_len;

	fp = fopen(context_file, "r");
	if (fp == NULL) {
		return -1;
	}

	while ((fgets(buf, sizeof(buf), fp)) != NULL) {
		buf_len = strlen(buf);

		if ((buf_len > 0) && (buf[buf_len - 1] == '\n')) {
			buf[buf_len - 1] = '\0';
		}

		bufp = buf;
		while ((bufp < buf + sizeof(buf)) &&
		       (*bufp != '\0') &&
		       g_ascii_isspace(*bufp)) {
			bufp++;
		}

		if (*bufp != '\0') {
			*context = strdup(bufp);
			if (*context == NULL) {
				goto out;
			}
			fclose(fp);
			return 0;
		}
	}
out:
	fclose(fp);
	errno = EBADF;
	return -1;
}
#endif /* WITH_SELINUX */

/* Exit, returning the proper status code based on a PAM error code. */
static int
fail_exit(int pam_retval)
{
	/* This is a local error.  Bail. */
	if (pam_retval == ERR_SHELL_INVALID) {
		exit(ERR_SHELL_INVALID);
	}

	if (pam_retval != PAM_SUCCESS) {
		/* Map the PAM error code to a local error code and return
		 * it to the parent process.  Trust the canceled flag before
		 * any PAM error codes. */
		if (app_data.canceled) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: exiting with status %d.\n",
				ERR_CANCELED);
#endif
			_exit(ERR_CANCELED);
		}
#ifdef DEBUG_USERHELPER
		g_print("userhelper: got PAM error %d.\n", pam_retval);
#endif
		switch (pam_retval) {
			case PAM_AUTH_ERR:
			case PAM_PERM_DENIED:
#ifdef DEBUG_USERHELPER
				g_print("userhelper: exiting with status %d.\n",
					ERR_PASSWD_INVALID);
#endif
				exit(ERR_PASSWD_INVALID);
				break;
			case PAM_AUTHTOK_LOCK_BUSY:
#ifdef DEBUG_USERHELPER
				g_print("userhelper: exiting with status %d.\n",
					ERR_LOCKS);
#endif
				exit(ERR_LOCKS);
				break;
			case PAM_CRED_INSUFFICIENT:
			case PAM_AUTHINFO_UNAVAIL:
#ifdef DEBUG_USERHELPER
				g_print("userhelper: exiting with status %d.\n",
					ERR_NO_RIGHTS);
#endif
				exit(ERR_NO_RIGHTS);
				break;
			case PAM_ABORT:
				/* fall through */
			default:
#ifdef DEBUG_USERHELPER
				g_print("userhelper: exiting with status %d.\n",
					ERR_UNK_ERROR);
#endif
				exit(ERR_UNK_ERROR);
				break;
		}
	}
	/* Just exit. */
#ifdef DEBUG_USERHELPER
	g_print("userhelper: exiting with status %d.\n", 0);
#endif
	_exit(0);
}

/* Read a string from stdin, and return a freshly-allocated copy, without
 * the end-of-line terminator if there was one, and with an optional
 * consolehelper message header removed. */
static char *
read_reply(FILE *fp)
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

static int
get_pam_string_item(pam_handle_t *pamh, int item, const char **ret)
{
	return pam_get_item(pamh, item, (const void**) ret);
}

/* A mixed-mode conversation function suitable for use with X. */
static int
converse_pipe(int num_msg, const struct pam_message **msg,
	      struct pam_response **resp, void *appdata_ptr)
{
	int count = 0;
	int responses = 0;
	struct pam_response *reply = NULL;
	char *noecho_message, *string;
	const char *user, *service;
	struct app_data *app_data = appdata_ptr;

	/* Pass on any hints we have to the consolehelper. */

	/* User. */
	if ((get_pam_string_item(app_data->pamh, PAM_USER,
				 &user) != PAM_SUCCESS) ||
	    (user == NULL) ||
	    (strlen(user) == 0)) {
		user = "root";
	}
#ifdef DEBUG_USERHELPER
	g_print("userhelper: sending user `%s'\n", user);
#endif
	fprintf(app_data->output, "%d %s\n", UH_USER, user);

	/* Service. */
	if (get_pam_string_item(app_data->pamh, PAM_SERVICE,
				&service) == PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending service `%s'\n", service);
#endif
		fprintf(app_data->output, "%d %s\n", UH_SERVICE_NAME, service);
	}

	/* Fallback allowed? */
#ifdef DEBUG_USERHELPER
	g_print("userhelper: sending fallback = %d.\n",
		app_data->fallback_allowed ? 1 : 0);
#endif
	fprintf(app_data->output, "%d %d\n",
		UH_FALLBACK_ALLOW, app_data->fallback_allowed ? 1 : 0);

	/* Banner. */
	if ((app_data->domain != NULL) && (app_data->banner != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending banner `%s'\n", app_data->banner);
#endif
		fprintf(app_data->output, "%d %s\n", UH_BANNER,
			dgettext(app_data->domain, app_data->banner));
	}

#ifdef USE_STARTUP_NOTIFICATION
	/* SN Name. */
	if ((app_data->domain != NULL) && (app_data->sn_name != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn name `%s'\n",
			app_data->sn_name);
#endif
		fprintf(app_data->output, "%d %s\n", UH_SN_NAME,
			dgettext(app_data->domain, app_data->sn_name));
	}

	/* SN Description. */
	if ((app_data->domain != NULL) && (app_data->sn_description != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn description `%s'\n",
			app_data->sn_description);
#endif
		fprintf(app_data->output, "%d %s\n", UH_SN_DESCRIPTION,
			dgettext(app_data->domain, app_data->sn_description));
	}

	/* SN WM Class. */
	if ((app_data->domain != NULL) && (app_data->sn_wmclass != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn wm_class `%s'\n",
			app_data->sn_wmclass);
#endif
		fprintf(app_data->output, "%d %s\n", UH_SN_WMCLASS,
			dgettext(app_data->domain, app_data->sn_wmclass));
	}

	/* SN BinaryName. */
	if ((app_data->domain != NULL) && (app_data->sn_binary_name != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn binary name `%s'\n",
			app_data->sn_binary_name);
#endif
		fprintf(app_data->output, "%d %s\n", UH_SN_BINARY_NAME,
			dgettext(app_data->domain, app_data->sn_binary_name));
	}

	/* SN IconName. */
	if ((app_data->domain != NULL) && (app_data->sn_icon_name != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn icon name `%s'\n",
			app_data->sn_icon_name);
#endif
		fprintf(app_data->output, "%d %s\n", UH_SN_ICON_NAME,
			dgettext(app_data->domain, app_data->sn_icon_name));
	}

	/* SN Workspace. */
	if ((app_data->domain != NULL) && (app_data->sn_workspace != -1)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn workspace %d.\n",
			app_data->sn_workspace);
#endif
		fprintf(app_data->output, "%d %d\n", UH_SN_WORKSPACE,
			app_data->sn_workspace);
	}
#endif

	/* We do a first pass on all items and output them, and then a second
	 * pass to read responses from the helper. */
	for (count = responses = 0; count < num_msg; count++) {
		switch (msg[count]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
				/* Spit out the prompt. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: sending prompt (echo on) ="
					" \"%s\".\n", msg[count]->msg);
#endif
				fprintf(app_data->output, "%d %s\n",
					UH_ECHO_ON_PROMPT, msg[count]->msg);
				responses++;
				break;
			case PAM_PROMPT_ECHO_OFF:
				/* If the prompt is for the user's password,
				 * indicate the user's name if we can.
				 * Otherwise, just output the prompt as-is. */
				if ((strncasecmp(msg[count]->msg,
						 "password",
						 8) == 0)) {
					noecho_message =
						g_strdup_printf(_("Password for %s"),
								user);
				} else {
					noecho_message = g_strdup(msg[count]->msg);
				}
#ifdef DEBUG_USERHELPER
				g_print("userhelper: sending prompt (no echo) ="
					" \"%s\".\n", noecho_message);
#endif
				fprintf(app_data->output, "%d %s\n",
					UH_ECHO_OFF_PROMPT, noecho_message);
				g_free(noecho_message);
				responses++;
				break;
			case PAM_TEXT_INFO:
				/* Text information strings are output
				 * verbatim. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: sending text = \"%s\".\n",
					msg[count]->msg);
#endif
				fprintf(app_data->output, "%d %s\n",
					UH_INFO_MSG, msg[count]->msg);
				break;
			case PAM_ERROR_MSG:
				/* Error message strings are output verbatim. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: sending error = \"%s\".\n",
					msg[count]->msg);
#endif
				fprintf(app_data->output, "%d %s\n",
					UH_ERROR_MSG, msg[count]->msg);
				break;
			default:
				/* Maybe the consolehelper can figure out what
				 * to do with this, because we sure can't. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: sending ??? = \"%s\".\n",
					msg[count]->msg);
#endif
				fprintf(app_data->output, "%d %s\n",
					UH_UNKNOWN_PROMPT, msg[count]->msg);
				break;
		}
	}

	/* Tell the consolehelper how many messages for which we expect to
	 * receive responses. */
#ifdef DEBUG_USERHELPER
	g_print("userhelper: sending expected response count = %d.\n",
		responses);
#endif
	fprintf(app_data->output, "%d %d\n", UH_EXPECT_RESP, responses);

	/* Tell the consolehelper that we're ready for it to do its thing. */
#ifdef DEBUG_USERHELPER
	g_print("userhelper: sending sync point.\n");
#endif
	fprintf(app_data->output, "%d\n", UH_SYNC_POINT);
	fflush(NULL);

	/* Now, for the second pass, allocate space for the responses and read
	 * the answers back. */
	reply = g_malloc0((num_msg + 1) * sizeof(struct pam_response));
	app_data->fallback_chosen = FALSE;

	/* First, handle the items which don't require answers. */
	count = 0;
	while (count < num_msg) {
		switch (msg[count]->msg_style) {
		case PAM_TEXT_INFO:
		case PAM_ERROR_MSG:
			/* Ignore it... */
			reply[count].resp_retcode = PAM_SUCCESS;
			break;
		default:
			break;
		}
		count++;
	}

	/* Now read responses until we hit a sync point or an EOF. */
	count = 0;
	do {
		string = read_reply(app_data->input);

		/* If we got nothing, and we expected data, then we're done. */
		if ((string == NULL) && (count < responses)) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: got %d responses, expected %d\n",
				count, responses);
#endif
			app_data->canceled = TRUE;
			g_free(reply);
			return PAM_ABORT;
		}

#ifdef DEBUG_USERHELPER
		g_print("userhelper: received string type %d, text \"%s\".\n",
			string[0], string[0] ? string + 1 : "");
#endif

		/* If we hit a sync point, we're done. */
		if (string[0] == UH_SYNC_POINT) {
			if (count != responses) {
				/* Whoa, not done yet! */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: got %d responses, "
					"expected %d\n", count, responses);
#endif
				g_free(reply);
				return PAM_CONV_ERR;
			}
			/* Okay, we're done. */
			g_free(string);
			break;
		}

#ifdef USE_STARTUP_NOTIFICATION
		/* If we got a desktop startup ID, set it. */
		if (string[0] == UH_SN_ID) {
			if (app_data->sn_id) {
				g_free(app_data->sn_id);
			}
			app_data->sn_id = string + 1;
			while ((app_data->sn_id[0] != '\0') &&
			       (g_ascii_isspace(app_data->sn_id[0]))) {
				app_data->sn_id++;
			}
			app_data->sn_id = g_strdup(app_data->sn_id);
			g_free(string);
#ifdef DEBUG_USERHELPER
			g_print("userhelper: startup id \"%s\"\n",
				app_data->sn_id);
#endif
			continue;
		}
#endif

		/* If the user chose to abort, do so. */
		if (string[0] == UH_CANCEL) {
			app_data->canceled = TRUE;
			g_free(string);
			g_free(reply);
#ifdef DEBUG_USERHELPER
			g_print("userhelper: canceling\n");
#endif
			return PAM_ABORT;
		}

		/* If the user chose to fallback, do so. */
		if (string[0] == UH_FALLBACK) {
			app_data->fallback_chosen = TRUE;
			g_free(string);
			g_free(reply);
#ifdef DEBUG_USERHELPER
			g_print("userhelper: falling back\n");
#endif
			return PAM_ABORT;
		}

		/* Find the first unanswered prompt. */
		while ((count < responses) &&
		       (msg[count]->msg_style != PAM_PROMPT_ECHO_ON) &&
		       (msg[count]->msg_style != PAM_PROMPT_ECHO_OFF)) {
			count++;
		}
		if (count >= responses) {
			/* Whoa, TMI! */
#ifdef DEBUG_USERHELPER
			g_print("userhelper: got %d responses, expected %d\n",
				count, responses);
#endif
			g_free(reply);
			return PAM_CONV_ERR;
		}

		/* Save this response. */
		reply[count].resp = string + 1;
		while ((reply[count].resp[0] != '\0') &&
		       (g_ascii_isspace(reply[count].resp[0]))) {
			reply[count].resp++;
		}
		reply[count].resp = g_strdup(reply[count].resp);
		reply[count].resp_retcode = PAM_SUCCESS;
#ifdef DEBUG_USERHELPER
		g_print("userhelper: got `%s'\n", reply[count].resp);
#endif
		g_free(string);
		count++;
	} while(1);

	/* Check that we got exactly the number of responses we were
	 * expecting. */
	if (count != responses) {
		/* Must be an error of some sort... */
#ifdef DEBUG_USERHELPER
		g_print("userhelper: got %d responses, expected %d\n",
			count, responses);
#endif
		g_free(reply);
		return PAM_CONV_ERR;
	}

	/* Return successfully. */
	if (resp != NULL) {
		*resp = reply;
	}
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

	get_pam_string_item(app_data->pamh, PAM_SERVICE, &service);
	get_pam_string_item(app_data->pamh, PAM_USER, &user);

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
	char *string;
	const char *user, *service;
	struct app_data *app_data = callback_data;

	/* Pass on any hints we have to the consolehelper. */
	fprintf(app_data->output, "%d %d\n",
		UH_FALLBACK_ALLOW, app_data->fallback_allowed ? 1 : 0);

	/* User. */
	if ((get_pam_string_item(app_data->pamh, PAM_USER,
				 &user) != PAM_SUCCESS) ||
	    (user == NULL) ||
	    (strlen(user) == 0)) {
		user = "root";
	}
#ifdef DEBUG_USERHELPER
	g_print("userhelper: sending user `%s'\n", user);
#endif
	fprintf(app_data->output, "%d %s\n", UH_USER, user);

	/* Service. */
	if (get_pam_string_item(app_data->pamh, PAM_SERVICE,
				&service) == PAM_SUCCESS) {
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
	fprintf(app_data->output, "%d\n", UH_SYNC_POINT);
	fflush(NULL);

	/* Now, for the second pass, allocate space for the responses and read
	 * the answers back. */
	i = 0;
	do {
		string = read_reply(app_data->input);

		if (string == NULL) {
			/* EOF: the child isn't going to give us any more
			 * information. */
			app_data->canceled = TRUE;
			return FALSE;
		}

		/* If we finished, we're done. */
		if (string[0] == UH_SYNC_POINT) {
			if (i < prompts_count) {
				/* Not enough information. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: not enough responses\n");
#endif
				g_free(string);
				return FALSE;
			}
			g_free(string);
			return TRUE;
		}

		/* If the user chose to abort, do so. */
		if (string[0] == UH_CANCEL) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: user canceled\n");
#endif
			g_free(string);
			app_data->canceled = TRUE;
			return FALSE;
		}

		/* If the user chose to fallback, do so. */
		if (string[0] == UH_FALLBACK) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: user fell back\n");
#endif
			g_free(string);
			app_data->fallback_chosen = TRUE;
			return FALSE;
		}

		/* Save this response. */
		prompts[i].free_value = g_free;
		prompts[i].value = g_strdup(string + 1);
#ifdef DEBUG_USERHELPER
		g_print("userhelper: got `%s'\n", prompts[i].value);
#endif
		g_free(string);
		i++;
	} while(1);

	/* If we got an unexpected number of responses, bail. */
	if (i != prompts_count) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: wrong number of responses\n");
#endif
		return FALSE;
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
static void
pipe_conv_exec_start(const struct pam_conv *conv)
{
	if (conv == &pipe_conv) {
		converse_pipe(0, NULL, NULL, &app_data);
		fprintf(app_data.output, "%d\n", UH_EXEC_START);
		fprintf(app_data.output, "%d\n", UH_SYNC_POINT);
		fflush(app_data.output);
#ifdef DEBUG_USERHELPER
		{
			int timeout = 5;
			g_print("userhelper: exec start\nuserhelper: pausing "
				"for %d seconds for debugging\n", timeout);
			sleep(timeout);
		}
#endif
	}
}
static void
pipe_conv_exec_fail(const struct pam_conv *conv)
{
	if (conv == &pipe_conv) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: exec failed\n");
#endif
		fprintf(app_data.output, "%d\n", UH_EXEC_FAILED);
		fprintf(app_data.output, "%d\n", UH_SYNC_POINT);
		fflush(app_data.output);
	}
}

/* Parse the passed-in GECOS string and set the globals to its broken-down
 * contents.  Note that the string *is* modified here, and the parsing is
 * performed using the convention obeyed by BSDish finger(1) under Linux.  */
static void
gecos_parse(char *gecos, struct gecos_data *parsed)
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
				dest = &parsed->full_name;
				break;
			case 1:
				dest = &parsed->office;
				break;
			case 2:
				dest = &parsed->office_phone;
				break;
			case 3:
				dest = &parsed->home_phone;
				break;
			case 4:
				dest = &parsed->site_info;
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
gecos_size(struct gecos_data *parsed)
{
	int len;

	len = 4; /* commas! */
	if (parsed->full_name != NULL) {
		len += strlen(parsed->full_name);
	}
	if (parsed->office != NULL) {
		len += strlen(parsed->office);
	}
	if (parsed->office_phone != NULL) {
		len += strlen(parsed->office_phone);
	}
	if (parsed->home_phone != NULL) {
		len += strlen(parsed->home_phone);
	}
	if (parsed->site_info != NULL) {
		len += strlen(parsed->site_info);
	}
	len++;

	return len;
}

/* Assemble a new gecos string. */
static char *
gecos_assemble(struct gecos_data *parsed)
{
	char *ret;
	int i;
	/* Construct the basic version of the string. */
	ret = g_strdup_printf("%s,%s,%s,%s,%s",
			      parsed->full_name ?: "",
			      parsed->office ?: "",
			      parsed->office_phone ?: "",
			      parsed->home_phone ?: "",
			      parsed->site_info ?: "");
	/* Strip off terminal commas. */
	i = strlen(ret);
	while ((i > 0) && (ret[i - 1] == ',')) {
		ret[i] = '\0';
		i--;
	}
	return ret;
}

/* Check if the passed-in shell is a valid shell according to getusershell(),
 * which is usually back-ended by /etc/shells.  Treat NULL or the empty string
 * as "/bin/sh", as is traditional. */
static gboolean
shell_valid(const char *shell_name)
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
			g_print("userhelper: got shell \"%s\"\n", shell);
#endif
			if ((shell_name != NULL) && (strlen(shell_name) > 0)) {
				if (strcmp(shell_name, shell) == 0) {
					found = TRUE;
					break;
				}
			} else {
				if (strcmp("/bin/sh", shell) == 0) {
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
		g_print("userhelper: set*id() failure: %s\n", strerror(errno));
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
		g_print("userhelper: still setgid()\n");
#endif
		exit(ERR_EXEC_FAILED);
	}
	/* Become the user who invoked us. */
	setreuid(getuid(), getuid());
	/* Yes, setuid() can fail. */
	if (geteuid() != getuid()) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: still setuid()\n");
#endif
		exit(ERR_EXEC_FAILED);
	}
}

/* Determine the name of the user whose ruid we're executing under.  For
 * SELinux, this is the user from the previous context. */
char *
get_user_from_ruid()
{
	struct passwd *pwd;
	char *ret;

	ret = NULL;

#ifdef WITH_SELINUX
	selinux_enabled = is_selinux_enabled();
	if (selinux_enabled) {
		context_t ctx;
		if (getprevcon(&old_context) < 0) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: i have no name\n");
#endif
			exit(ERR_UNK_ERROR);
		}
		ctx = context_new(old_context);
		ret = g_strdup(context_user_get(ctx));
		context_free(ctx);
	}
#endif
	if (ret == NULL) {
		/* Now try to figure out who called us. */
		pwd = getpwuid(getuid());
		if ((pwd != NULL) && (pwd->pw_name != NULL)) {
			ret = g_strdup(pwd->pw_name);
		} else {
			/* I have no name and I must have one. */
#ifdef DEBUG_USERHELPER
			g_print("userhelper: i have no name\n");
#endif
			exit(ERR_UNK_ERROR);
		}
	}

	return ret;
}

/* Determine the name of the user as whom we must authenticate. */
char *
get_user_for_auth(shvarFile *s)
{
	char *ret = NULL;
	char *ruid_user, *configured_user;

	ruid_user = get_user_from_ruid();

#ifdef WITH_SELINUX
	if (selinux_enabled) {
		security_context_t defcontext = NULL;
		char *apps_role, *apps_type;
		context_t ctx;

		if (get_init_context(CONTEXT_FILE, &defcontext) == 0) {
			ctx = context_new(defcontext);
		} else {
			ctx = context_new(old_context);
		}
		context_user_set(ctx, ruid_user);
		apps_role = svGetValue(s, "ROLE");
		apps_type = svGetValue(s, "TYPE");
		if (apps_role != NULL) { 
			context_type_set(ctx, apps_type); /* XXX */
		}
		if (apps_type != NULL) {
			context_role_set(ctx, apps_role); /* XXX */
		}
		freecon(defcontext);
		defcontext = NULL;
		freecon(old_context);
		old_context = NULL;

		new_context = strdup(context_str(ctx));
		context_free(ctx);
		ret = ruid_user;
	}
#endif
	if (ret == NULL) {
		/* Determine who we should authenticate as.  If not specified,
		 * or if "<user>" is specified, we authenticate as the invoking
		 * user, otherwise we authenticate as the specified user (which
		 * is usually root, but could conceivably be someone else). */
		configured_user = svGetValue(s, "USER");
		if (configured_user == NULL) {
			ret = ruid_user;
		} else
		if (strcmp(configured_user, "<user>") == 0) {
			free(configured_user);
			ret = ruid_user;
		} else {
			ret = configured_user;
		}
	}

	if (ret != NULL) {
		if (ruid_user != ret) {
			free(ruid_user);
		}
		return ret;
	}

	return NULL;
}

/* Change the user's password using the indicated conversation function and
 * application data (which includes the ability to cancel if the user requests
 * it.  For this task, we don't retry on failure. */
static void
passwd(const char *user, struct pam_conv *conv, struct app_data *data)
{
	int retval;
	pam_handle_t *pamh;

	retval = pam_start("passwd", user, conv, &pamh);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_start() failed\n");
#endif
		fail_exit(retval);
	}

	retval = pam_set_item(pamh, PAM_RUSER, user);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_set_item() failed\n");
#endif
		fail_exit(retval);
	}

#ifdef DEBUG_USERHELPER
	g_print("userhelper: changing password for \"%s\"\n", user);
#endif
	retval = pam_chauthtok(pamh, 0);
#ifdef DEBUG_USERHELPER
	g_print("userhelper: PAM retval = %d (%s)\n", retval,
		pam_strerror(pamh, retval));
#endif

	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_chauthtok() failed\n");
#endif
		fail_exit(retval);
	}

	retval = pam_end(pamh, PAM_SUCCESS);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_end() failed\n");
#endif
		fail_exit(retval);
	}
	exit(0);
}

/* We're here to change the user's non-security information.  PAM doesn't
 * provide an interface to do this, because it's not PAM's job to manage this
 * stuff, so farm it out to a different library. */
static void
chfn(const char *user, struct pam_conv *conv, lu_prompt_fn *prompt,
     struct app_data *data,
     const char *new_full_name, const char *new_office,
     const char *new_office_phone, const char *new_home_phone,
     const char *new_shell)
{
	char *new_gecos, *gecos, *old_shell;
	struct gecos_data parsed_gecos;
	const char *authed_user;
	struct lu_context *context;
	struct lu_ent *ent;
	struct lu_error *error;
	GValueArray *values;
	GValue *value, val;
	int tryagain = 3, retval;
	gboolean ret;

	/* Verify that the fields we were given on the command-line
	 * are sane (i.e., contain no forbidden characters). */
	if (new_full_name && strpbrk(new_full_name, ":,=")) {
		exit(ERR_FIELDS_INVALID);
	}
	if (new_office && strpbrk(new_office, ":,=")) {
		exit(ERR_FIELDS_INVALID);
	}
	if (new_office_phone && strpbrk(new_office_phone, ":,=")) {
		exit(ERR_FIELDS_INVALID);
	}
	if (new_home_phone && strpbrk(new_home_phone, ":,=")) {
		exit(ERR_FIELDS_INVALID);
	}

	/* Start up PAM to authenticate the user, this time pretending
	 * we're "chfn". */
	retval = pam_start("chfn", user, conv, &app_data.pamh);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_start() failed\n");
#endif
		fail_exit(retval);
	}

	/* Set the requesting user. */
	retval = pam_set_item(app_data.pamh, PAM_RUSER, user);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_set_item() failed\n");
#endif
		fail_exit(retval);
	}

	/* Try to authenticate the user. */
	do {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: about to authenticate \"%s\"\n", user);
#endif
		retval = pam_authenticate(app_data.pamh, 0);
#ifdef DEBUG_USERHELPER
		g_print("userhelper: PAM retval = %d (%s)\n", retval,
			pam_strerror(app_data.pamh, retval));
#endif
		tryagain--;
	} while ((retval != PAM_SUCCESS) &&
		 (retval != PAM_CONV_ERR) &&
		 !app_data.canceled &&
		 (tryagain > 0));
	/* If we didn't succeed, bail. */
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam authentication failed\n");
#endif
		pam_end(app_data.pamh, retval);
		fail_exit(retval);
	}

	/* Verify that the authenticated user is the user we started
	 * out trying to authenticate. */
	retval = get_pam_string_item(app_data.pamh, PAM_USER,
				     &authed_user);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: no pam user set\n");
#endif
		pam_end(app_data.pamh, retval);
		fail_exit(retval);
	}

	/* At some point this check will go away. */
	if (strcmp(user, authed_user) != 0) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: username(%s) != authuser(%s)\n",
			user, authed_user);
#endif
		exit(ERR_UNK_ERROR);
	}

	/* Check if the user is allowed to change her information at
	 * this time, on this machine, yadda, yadda, yadda.... */
	retval = pam_acct_mgmt(app_data.pamh, 0);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_acct_mgmt() failed\n");
#endif
		pam_end(app_data.pamh, retval);
		fail_exit(retval);
	}

	/* Let's get to it.  Start up libuser. */
	context = lu_start(user, lu_user, NULL, NULL, prompt,
			   (gpointer)&app_data, &error);
	if (context == NULL) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: libuser startup error\n");
#endif
		pam_end(app_data.pamh, PAM_ABORT);
		fail_exit(PAM_ABORT);
	}
	if (error != NULL) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: libuser startup error: %s\n",
			lu_strerror(error));
#endif
		pam_end(app_data.pamh, PAM_ABORT);
		fail_exit(PAM_ABORT);
	}

	/* Look up the user's record. */
	ent = lu_ent_new();
	ret = lu_user_lookup_name(context, user, ent, &error);
	if (ret != TRUE) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: libuser doesn't know the user \"%s\"\n",
			user);
#endif
		pam_end(app_data.pamh, PAM_ABORT);
		fail_exit(PAM_ABORT);
	}
	if (error != NULL) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: libuser startup error: %s\n",
			lu_strerror(error));
#endif
		pam_end(app_data.pamh, PAM_ABORT);
		fail_exit(PAM_ABORT);
	}

	/* Pull up the user's GECOS data, and split it up. */
	memset(&parsed_gecos, 0, sizeof(parsed_gecos));
	values = lu_ent_get(ent, LU_GECOS);
	if (values != NULL) {
		value = g_value_array_get_nth(values, 0);
		if (G_VALUE_HOLDS_STRING(value)) {
			gecos = g_value_dup_string(value);
		} else
		if (G_VALUE_HOLDS_LONG(value)) {
			gecos = g_strdup_printf("%ld", g_value_get_long(value));
		} else {
			gecos = NULL;
			g_assert_not_reached();
		}
		gecos_parse(gecos, &parsed_gecos);
	}

	/* Override any new values we have. */
	if (new_full_name != NULL) {
		if (parsed_gecos.full_name != NULL) {
			g_free(parsed_gecos.full_name);
		}
		parsed_gecos.full_name = g_strdup(new_full_name);
	}
	if (new_office != NULL) {
		if (parsed_gecos.office != NULL) {
			g_free(parsed_gecos.office);
		}
		parsed_gecos.office = g_strdup(new_office);
	}
	if (new_office_phone != NULL) {
		if (parsed_gecos.office_phone != NULL) {
			g_free(parsed_gecos.office_phone);
		}
		parsed_gecos.office_phone = g_strdup(new_office_phone);
	}
	if (new_home_phone != NULL) {
		if (parsed_gecos.home_phone != NULL) {
			g_free(parsed_gecos.home_phone);
		}
		parsed_gecos.home_phone = g_strdup(new_home_phone);
	}

	/* Verify that the strings we got passed are not too long. */
	if (gecos_size(&parsed_gecos) > GECOS_LENGTH) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: user gecos too long %d > %d\n",
			gecos_size(&parsed_gecos), GECOS_LENGTH);
#endif
		lu_ent_free(ent);
		lu_end(context);
		pam_end(app_data.pamh, PAM_ABORT);
		exit(ERR_FIELDS_INVALID);
	}

	/* Build a new value for the GECOS data. */
	new_gecos = gecos_assemble(&parsed_gecos);

	/* We don't need the user's current GECOS anymore, so clear
	 * out the value and set our own in the in-memory structure. */
	memset(&val, 0, sizeof(val));
	g_value_init(&val, G_TYPE_STRING);

	lu_ent_clear(ent, LU_GECOS);
	g_value_set_string(&val, new_gecos);
	lu_ent_add(ent, LU_GECOS, &val);

	/* While we're at it, set the individual data items as well. */
	lu_ent_clear(ent, LU_COMMONNAME);
	g_value_set_string(&val, parsed_gecos.full_name);
	lu_ent_add(ent, LU_COMMONNAME, &val);

	lu_ent_clear(ent, LU_ROOMNUMBER);
	g_value_set_string(&val, parsed_gecos.office);
	lu_ent_add(ent, LU_ROOMNUMBER, &val);

	lu_ent_clear(ent, LU_TELEPHONENUMBER);
	g_value_set_string(&val, parsed_gecos.office_phone);
	lu_ent_add(ent, LU_TELEPHONENUMBER, &val);

	lu_ent_clear(ent, LU_HOMEPHONE);
	g_value_set_string(&val, parsed_gecos.home_phone);
	lu_ent_add(ent, LU_HOMEPHONE, &val);

	/* If we're here to change the user's shell, too, do that while we're
	 * in here, assuming that chsh and chfn have identical PAM
	 * configurations. */
	if (new_shell != NULL) {
		/* Check that the user's current shell is valid, and that she
		 * is not attempting to change to an invalid shell. */
		values = lu_ent_get(ent, LU_LOGINSHELL);
		if (values != NULL) {
			value = g_value_array_get_nth(values, 0);
			if (G_VALUE_HOLDS_STRING(value)) {
				old_shell= g_value_dup_string(value);
			} else
			if (G_VALUE_HOLDS_LONG(value)) {
				old_shell = g_strdup_printf("%ld", g_value_get_long(value));
			} else {
				old_shell = NULL;
				g_assert_not_reached();
			}
		} else {
			old_shell = g_strdup("/bin/sh");
		}

#ifdef DEBUG_USERHELPER
		g_print("userhelper: current shell \"%s\"\n", old_shell);
		g_print("userhelper: new shell \"%s\"\n", new_shell);
#endif
		/* If the old or new shell are invalid, then
		 * the user doesn't get to make the change. */
		if (!shell_valid(new_shell) || !shell_valid(old_shell)) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: bad shell value\n");
#endif
			lu_ent_free(ent);
			lu_end(context);
			pam_end(app_data.pamh, PAM_ABORT);
			fail_exit(ERR_SHELL_INVALID);
		}

		/* Set the shell to the new value. */
		lu_ent_clear(ent, LU_LOGINSHELL);
		g_value_set_string(&val, new_shell);
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
		g_print("userhelper: libuser save failed\n");
#endif
		fail_exit(PAM_ABORT);
	}
	if (error != NULL) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: libuser save error: %s\n",
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

static void
wrap(const char *user, const char *program,
     struct pam_conv *conv, lu_prompter_fn *prompt,
     int argc, char **argv)
{
	/* We're here to wrap the named program.  After authenticating as the
	 * user given in the console.apps configuration file, execute the
	 * command given in the console.apps file. */
	char *constructed_path;
	char *apps_filename;
	char *user_pam;
	const char *auth_user;
	char *apps_banner, *apps_domain, *apps_sn = NULL;
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
	 * XAUTHORITY is not initially copied -- we don't want to let attackers
	 * get at others' X authority records -- we restore XAUTHORITY below
	 * *after* successfully authenticating, or abandoning authentication in
	 * order to run the wrapped program as the invoking user. */
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

	/* Open the console.apps configuration file for this wrapped program,
	 * and read settings from it. */
	apps_filename = g_strdup_printf(SYSCONFDIR
					"/security/console.apps/%s",
					progname);
	s = svNewFile(apps_filename);

	/* If the file is world-writable, or isn't a regular file, or couldn't
	 * be opened, just exit.  We don't want to alert an attacker that the
	 * service name is invalid. */
	if ((s == NULL) ||
	    (fstat(s->fd, &sbuf) == -1) ||
	    !S_ISREG(sbuf.st_mode) ||
	    (sbuf.st_mode & S_IWOTH)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: bad file permissions\n");
#endif
		exit(ERR_UNK_ERROR);
	}

	user_pam = get_user_for_auth(s);

	/* Read the path to the program to run. */
	constructed_path = svGetValue(s, "PROGRAM");
	if (!constructed_path || constructed_path[0] != '/') {
		/* Criminy....  The system administrator didn't give us an
		 * absolute path to the program!  Guess either /usr/sbin or
		 * /sbin, and then give up if we don't find anything by that
		 * name in either of those directories.  FIXME: we're a setuid
		 * app, so access() may not be correct here, as it may give
		 * false negatives.  But then, it wasn't an absolute path. */
		constructed_path = g_strdup_printf("/usr/sbin/%s", progname);
		if (access(constructed_path, X_OK) != 0) {
			/* Try the second directory. */
			strcpy(constructed_path, "/sbin/");
			strcat(constructed_path, progname);
			if (access(constructed_path, X_OK)) {
				/* Nope, not there, either. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: couldn't find "
					"wrapped binary\n");
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
		g_print("userhelper: no user named %s exists\n", user_pam);
#endif
		exit(ERR_NO_USER);
	}

	/* If the user we're authenticating as has root's UID, then it's
	 * safe to let them use HOME=~root. */
	if (pw->pw_uid == 0) {
		setenv("HOME", g_strdup(pw->pw_dir), 1);
	} else {
		/* Otherwise, if they had a reasonable value for HOME, let them
		 * use it. */
		if (env_home != NULL) {
			setenv("HOME", env_home, 1);
		} else {
			/* Otherwise, set HOME to the user's home directory. */
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
	apps_domain = svGetValue(s, "DOMAIN");
	if ((apps_domain != NULL) && (strlen(apps_domain) > 0)) {
		bindtextdomain(apps_domain, DATADIR "/locale");
		bind_textdomain_codeset(apps_domain, "UTF-8");
		app_data.domain = apps_domain;
	}
	if (app_data.domain == NULL) {
		apps_domain = svGetValue(s, "BANNER_DOMAIN");
		if ((apps_domain != NULL) &&
		    (strlen(apps_domain) > 0)) {
			bindtextdomain(apps_domain, DATADIR "/locale");
			bind_textdomain_codeset(apps_domain, "UTF-8");
			app_data.domain = apps_domain;
		}
	}
	if (app_data.domain == NULL) {
		app_data.domain = progname;
	}
#ifdef USE_STARTUP_NOTIFICATION
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_NAME");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		app_data.sn_name = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_DESCRIPTION");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		app_data.sn_description = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_WMCLASS");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		app_data.sn_wmclass = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_BINARY_NAME");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		app_data.sn_binary_name = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_ICON_NAME");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		app_data.sn_icon_name = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_WORKSPACE");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		app_data.sn_workspace = atoi(apps_sn);
	}
#endif

	/* Now we're done reading the file. Close it. */
	svCloseFile(s);

	/* Start up PAM to authenticate the specified user. */
	retval = pam_start(progname, user_pam, conv, &app_data.pamh);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_start() failed\n");
#endif
		fail_exit(retval);
	}

	/* Set the requesting user. */
	retval = pam_set_item(app_data.pamh, PAM_RUSER, user_name);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_set_item() failed\n");
#endif
		fail_exit(retval);
	}

	/* Try to authenticate the user. */
	do {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: authenticating \"%s\"\n",
			user_pam);
#endif
		retval = pam_authenticate(app_data.pamh, 0);
#ifdef DEBUG_USERHELPER
		g_print("userhelper: PAM retval = %d (%s)\n", retval,
			pam_strerror(app_data.pamh, retval));
#endif
		tryagain--;
	} while ((retval != PAM_SUCCESS) && tryagain &&
		 !app_data.fallback_chosen && !app_data.canceled);

	if (retval != PAM_SUCCESS) {
		pam_end(app_data.pamh, retval);
		if (app_data.canceled) {
			fail_exit(retval);
		} else
		if (app_data.fallback_allowed) {
			/* Reset the user's environment so that the
			 * application can run normally. */
			argv[optind - 1] = progname;
			environ = environ_save;
			become_normal(user_name);
			if (app_data.input != NULL) {
				fflush(app_data.input);
				fcntl(UH_INFILENO, F_SETFD, FD_CLOEXEC);
			}
			if (app_data.output != NULL) {
				fflush(app_data.output);
				fcntl(UH_OUTFILENO, F_SETFD, FD_CLOEXEC);
			}
			pipe_conv_exec_start(conv);
#ifdef USE_STARTUP_NOTIFICATION
			if (app_data.sn_id) {
#ifdef DEBUG_USERHELPER
				g_print("userhelper: setting "
					"DESKTOP_STARTUP_ID =\"%s\"\n",
					app_data.sn_id);
#endif
				setenv("DESKTOP_STARTUP_ID",
				       app_data.sn_id, 1);
			}
#endif
#ifdef WITH_SELINUX
			setup_selinux_exec(constructed_path);
#endif
			execv(constructed_path, argv + optind - 1);
			pipe_conv_exec_fail(conv);
			exit(ERR_EXEC_FAILED);
		} else {
			/* Well, we tried. */
			fail_exit(retval);
		}
	}

	/* Verify that the authenticated user is the user we started
	 * out trying to authenticate. */
	retval = get_pam_string_item(app_data.pamh, PAM_USER,
				     &auth_user);
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
		g_print("userhelper: no user named %s exists\n",
			user_pam);
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
				g_print("userhelper: setting %s\n",
					*env_pam);
#endif
				putenv(g_strdup(*env_pam));
				env_pam++;
			}

			argv[optind - 1] = progname;
#ifdef DEBUG_USERHELPER
			g_print("userhelper: about to exec \"%s\"\n",
				constructed_path);
#endif
			become_super();
			if (app_data.input != NULL) {
				fflush(app_data.input);
				fcntl(UH_INFILENO, F_SETFD, FD_CLOEXEC);
			}
			if (app_data.output != NULL) {
				fflush(app_data.output);
				fcntl(UH_OUTFILENO, F_SETFD, FD_CLOEXEC);
			}
			pipe_conv_exec_start(conv);
#ifdef USE_STARTUP_NOTIFICATION
			if (app_data.sn_id) {
#ifdef DEBUG_USERHELPER
				g_print("userhelper: setting "
					"DESKTOP_STARTUP_ID =\"%s\"\n",
					app_data.sn_id);
#endif
				setenv("DESKTOP_STARTUP_ID",
				       app_data.sn_id, 1);
			}
#endif
#ifdef WITH_SELINUX
			setup_selinux_exec(constructed_path);
#endif
			execv(constructed_path, argv + optind - 1);
			pipe_conv_exec_fail(conv);
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
		g_print("userhelper: about to exec \"%s\"\n",
			constructed_path);
#endif
		become_super();
		if (app_data.input != NULL) {
			fflush(app_data.input);
			fcntl(UH_INFILENO, F_SETFD, FD_CLOEXEC);
		}
		if (app_data.output != NULL) {
			fflush(app_data.output);
			fcntl(UH_OUTFILENO, F_SETFD, FD_CLOEXEC);
		}
		pipe_conv_exec_start(conv);
#ifdef USE_STARTUP_NOTIFICATION
		if (app_data.sn_id) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: setting "
				"DESKTOP_STARTUP_ID =\"%s\"\n",
				app_data.sn_id);
#endif
			setenv("DESKTOP_STARTUP_ID", app_data.sn_id, 1);
		}
#endif
#ifdef WITH_SELINUX
		setup_selinux_exec(constructed_path);
#endif
		execv(constructed_path, argv + optind - 1);
		pipe_conv_exec_fail(conv);
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
	char *wrapped_program = NULL;
	struct passwd *pw;
	struct pam_conv *conv;
	lu_prompt_fn *prompt;
	char **environ_save;
	char *user_name; /* current user, as determined by real uid */
	int f_flag;	 /* -f flag = change full name */
	int o_flag;	 /* -o flag = change office name */
	int p_flag;	 /* -p flag = change office phone */
	int h_flag;	 /* -h flag = change home phone number */
	int c_flag;	 /* -c flag = change password */
	int s_flag;	 /* -s flag = change shell */
	int t_flag;	 /* -t flag = direct text-mode -- exec'ed */
	int w_flag;	 /* -w flag = act as a wrapper for next * args */
	const char *new_full_name;
	const char *new_office;
	const char *new_office_phone;
	const char *new_home_phone;
	const char *new_shell;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, DATADIR "/locale");
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	if (geteuid() != 0) {
		fprintf(stderr, _("userhelper must be setuid root\n"));
#ifdef DEBUG_USERHELPER
		g_print("userhelper: not setuid\n");
#endif
		exit(ERR_NO_RIGHTS);
	}

	f_flag = 0;
	o_flag = 0;
	p_flag = 0;
	h_flag = 0;
	c_flag = 0;
	s_flag = 0;
	t_flag = 0;
	w_flag = 0;
	new_full_name = NULL;
	new_office = NULL;
	new_office_phone = NULL;
	new_home_phone = NULL;
	new_shell = NULL;

	while ((w_flag == 0) &&
	       (arg = getopt(argc, argv, "f:o:p:h:s:ctw:")) != -1) {
		/* We process no arguments after -w program; those are passed
		 * on to a wrapped program. */
		switch (arg) {
			case 'f':
				/* Full name. */
				f_flag++;
				new_full_name = optarg;
				break;
			case 'o':
				/* Office. */
				o_flag++;
				new_office = optarg;
				break;
			case 'h':
				/* Home phone. */
				h_flag++;
				new_home_phone = optarg;
				break;
			case 'p':
				/* Office phone. */
				p_flag++;
				new_office_phone = optarg;
				break;
			case 's':
				/* Change shell flag. */
				s_flag++;
				new_shell = optarg;
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
				wrapped_program = optarg;
				break;
			default:
#ifdef DEBUG_USERHELPER
				g_print("userhelper: invalid call: "
					"unknown option\n");
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
		g_print("userhelper: invalid call: "
			"invalid combination of options\n");
#endif
		exit(ERR_INVALID_CALL);
	}

	/* Determine which conversation function to use. */
	if (t_flag) {
		/* We were told to use text mode. */
		if (isatty(STDIN_FILENO)) {
			/* We have a controlling tty on which we can disable
			 * echoing, so use the text conversation method. */
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
			g_print("userhelper: invalid call\n");
#endif
			exit(ERR_INVALID_CALL);
		}
		conv = &pipe_conv;
		prompt = &prompt_pipe;
	}

	user_name = get_user_from_ruid();
#ifdef DEBUG_USERHELPER
	g_print("userhelper: current user is %s\n", user_name);
#endif

	/* If we didn't get the -w flag, the last argument can be a user's
	 * name. */
	if (w_flag == 0) {
		if ((getuid() == 0) && (argc == optind + 1)) {
			/* We were started by the superuser, so accept the
			 * user's name. */
			user_name = g_strdup(argv[optind]);
#ifdef DEBUG_USERHELPER
			g_print("userhelper: modifying account data for %s\n",
				user_name);
#endif
		}

		/* Verify that the user exists. */
		pw = getpwnam(user_name);
		if ((pw == NULL) || (pw->pw_name == NULL)) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: user %s doesn't exist\n",
				user_name);
#endif
			exit(ERR_NO_USER);
		}
	}

	/* Change password? */
	if (c_flag) {
		passwd(user_name, conv, &app_data);
		g_assert_not_reached();
	}

	/* Change GECOS data or shell? */
	if (SHELL_FLAGS) {
		chfn(user_name, conv, prompt, &app_data,
		     new_full_name, new_office,
		     new_office_phone, new_home_phone,
		     new_shell);
		g_assert_not_reached();
	}

	/* Wrap some other program? */
	if (w_flag) {
		wrap(user_name, wrapped_program, conv, prompt, &app_data,
		     argc, argv);
		g_assert_not_reached();
	}

	/* Not reached. */
	g_assert_not_reached();
	exit(0);
}
