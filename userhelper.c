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
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libintl.h>
#include <locale.h>
#include <pwd.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <glib.h>
#include <glib-object.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#include <selinux/context.h>
#include <selinux/get_context_list.h>
#include <selinux/flask.h>
#include <selinux/av_permissions.h>

static gboolean selinux_enabled = FALSE;
static security_context_t new_context = NULL; /* our target security context */

#endif

#include "shvar.h"
#include "userhelper.h"

/* We manipulate the environment directly, so we have to declare (but not
 * define) the right variable here. */
extern char **environ;

/* A structure type which we use to carry psuedo-global data around with us. */
struct app_data {
	pam_handle_t *pamh;
	gboolean fallback_allowed, fallback_chosen, canceled;
	FILE *input, *output;
	const char *banner, *domain;
#ifdef USE_STARTUP_NOTIFICATION
	const char *sn_name, *sn_description, *sn_wmclass;
	const char *sn_binary_name, *sn_icon_name;
	char *sn_id;
	int sn_workspace;
#endif
};

#ifdef WITH_SELINUX
static int checkAccess(unsigned int selaccess) {
  int status=-1;
  security_context_t user_context;
  if( getprevcon(&user_context)==0 ) {
    struct av_decision avd;
    int retval = security_compute_av(user_context,
				     user_context,
				     SECCLASS_PASSWD,
				     selaccess,
				     &avd);
	  
    if ((retval == 0) && 
	((selaccess & avd.allowed) == selaccess)) {
      status=0;
    } 
    freecon(user_context);
  }

  if (status != 0 && security_getenforce()==0) {
      status=0;
  }
  return status;
}

/*
 * setup_selinux_exec()
 *
 * Set the new context to be transitioned to after the next exec(), or exit.
 *
 * in:		The name of a path, used in a debugging message.
 * out:		nothing
 * return:	0 on success, -1 on failure.
 */
static int
setup_selinux_exec(char *constructed_path)
{
  int status=0;
  if (selinux_enabled) {
#ifdef DEBUG_USERHELPER
    g_print("userhelper: exec \"%s\" with %s context\n",
	    constructed_path, new_context);
#endif
    if (setexeccon(new_context) < 0) {
      syslog(LOG_NOTICE,
	     _("Could not set exec context to %s.\n"),
	     new_context);
      fprintf(stderr,
	      _("Could not set exec context to %s.\n"),
	      new_context);
      if (security_getenforce() > 0)
	status=-1;
    }
    if (new_context) {
      freecon(new_context);
      new_context = NULL;
    }
  }
  return status;
}

/*
 * get_init_context()
 *
 * Read the name of a context from the given file.
 *
 * in:		The name of a file.
 * out:		The CONTEXT name listed in the file.
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

		/* trim off terminating newline */
		if ((buf_len > 0) && (buf[buf_len - 1] == '\n')) {
			buf[buf_len - 1] = '\0';
		}

		/* trim off terminating whitespace */
		buf_len = strlen(buf);
		while ((buf_len > 0) && (g_ascii_isspace(buf[buf_len - 1]))) {
			buf[buf_len - 1] = '\0';
			buf_len--;
		}

		/* skip initial whitespace */
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
static void selinux_init(shvarFile *s) {
	security_context_t defcontext=NULL;
	char *apps_role, *apps_type;
	context_t ctx;
	char context_file[PATH_MAX];
	
	security_context_t old_context = NULL; /* our original securiy context */
	if (getprevcon(&old_context) < 0) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: i have no name\n");
#endif
		exit(ERR_UNK_ERROR);
	}
	ctx = context_new(old_context);
	freecon(old_context);

	/* Assume userhelper's default context, if the context file
	 * contains one.  Just in case policy changes, we read the
	 * default context from a file instead of hard-coding it. */
	snprintf(context_file, PATH_MAX, "%s/%s",selinux_contexts_path(), "userhelper_context");
	
	if (get_init_context(context_file, &defcontext) == 0) {
		context_free(ctx);
		ctx = context_new(defcontext);
		freecon(defcontext);
		defcontext = NULL;
	} 
	/* Optionally change the role and type of the next context, per
	 * the service-specific userhelper configuration file. */
	apps_role = svGetValue(s, "ROLE");
	apps_type = svGetValue(s, "TYPE");
	if (apps_role != NULL) {
		context_role_set(ctx, apps_role);
	}
	if (apps_type != NULL) {
		context_type_set(ctx, apps_type);
	}
	context_user_set(ctx, "root");

	new_context = strdup(context_str(ctx));
	context_free(ctx);
#ifdef DEBUG_USERHELPER
	g_print("userhelper: context = '%s'\n", new_context);
#endif
	return;
}
#endif /* WITH_SELINUX */

/* Exit, returning the proper status code based on a PAM error code. */
static int
fail_exit(struct app_data *data, int pam_retval)
{
	/* This is a local error.  Bail. */
	if (pam_retval == ERR_SHELL_INVALID) {
		exit(ERR_SHELL_INVALID);
	}

	if (pam_retval != PAM_SUCCESS) {
		/* Map the PAM error code to a local error code and return
		 * it to the parent process.  Trust the canceled flag before
		 * any PAM error codes. */
		if (data->canceled) {
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
	char buffer[BUFSIZ], *check;
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
	int count, expected_responses, received_responses;
	struct pam_response *reply;
	char *noecho_message, *string;
	const char *user, *service;
	struct app_data *data = appdata_ptr;

	/* Pass on any hints we have to the consolehelper. */

	/* User. */
	if ((get_pam_string_item(data->pamh, PAM_USER, &user) != PAM_SUCCESS) ||
	    (user == NULL) ||
	    (strlen(user) == 0)) {
		user = "root";
	}
#ifdef DEBUG_USERHELPER
	g_print("userhelper: sending user `%s'\n", user);
#endif
	fprintf(data->output, "%d %s\n", UH_USER, user);

	/* Service. */
	if (get_pam_string_item(data->pamh, PAM_SERVICE,
				&service) == PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending service `%s'\n", service);
#endif
		fprintf(data->output, "%d %s\n", UH_SERVICE_NAME, service);
	}

	/* Fallback allowed? */
#ifdef DEBUG_USERHELPER
	g_print("userhelper: sending fallback = %d.\n",
		data->fallback_allowed ? 1 : 0);
#endif
	fprintf(data->output, "%d %d\n",
		UH_FALLBACK_ALLOW, data->fallback_allowed ? 1 : 0);

	/* Banner. */
	if ((data->domain != NULL) && (data->banner != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending banner `%s'\n", data->banner);
#endif
		fprintf(data->output, "%d %s\n", UH_BANNER,
			dgettext(data->domain, data->banner));
	}

#ifdef USE_STARTUP_NOTIFICATION
	/* SN Name. */
	if ((data->domain != NULL) && (data->sn_name != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn name `%s'\n",
			data->sn_name);
#endif
		fprintf(data->output, "%d %s\n", UH_SN_NAME,
			dgettext(data->domain, data->sn_name));
	}

	/* SN Description. */
	if ((data->domain != NULL) && (data->sn_description != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn description `%s'\n",
			data->sn_description);
#endif
		fprintf(data->output, "%d %s\n", UH_SN_DESCRIPTION,
			dgettext(data->domain, data->sn_description));
	}

	/* SN WM Class. */
	if ((data->domain != NULL) && (data->sn_wmclass != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn wm_class `%s'\n",
			data->sn_wmclass);
#endif
		fprintf(data->output, "%d %s\n", UH_SN_WMCLASS,
			dgettext(data->domain, data->sn_wmclass));
	}

	/* SN BinaryName. */
	if ((data->domain != NULL) && (data->sn_binary_name != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn binary name `%s'\n",
			data->sn_binary_name);
#endif
		fprintf(data->output, "%d %s\n", UH_SN_BINARY_NAME,
			dgettext(data->domain, data->sn_binary_name));
	}

	/* SN IconName. */
	if ((data->domain != NULL) && (data->sn_icon_name != NULL)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn icon name `%s'\n",
			data->sn_icon_name);
#endif
		fprintf(data->output, "%d %s\n", UH_SN_ICON_NAME,
			dgettext(data->domain, data->sn_icon_name));
	}

	/* SN Workspace. */
	if ((data->domain != NULL) && (data->sn_workspace != -1)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: sending sn workspace %d.\n",
			data->sn_workspace);
#endif
		fprintf(data->output, "%d %d\n", UH_SN_WORKSPACE,
			data->sn_workspace);
	}
#endif

	/* We do a first pass on all items and output them, and then a second
	 * pass to read responses from the helper. */
	for (count = expected_responses = 0; count < num_msg; count++) {
		switch (msg[count]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
				/* Spit out the prompt. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: sending prompt (echo on) ="
					" \"%s\".\n", msg[count]->msg);
#endif
				fprintf(data->output, "%d %s\n",
					UH_ECHO_ON_PROMPT, msg[count]->msg);
				expected_responses++;
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
				fprintf(data->output, "%d %s\n",
					UH_ECHO_OFF_PROMPT, noecho_message);
				g_free(noecho_message);
				expected_responses++;
				break;
			case PAM_TEXT_INFO:
				/* Text information strings are output
				 * verbatim. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: sending text = \"%s\".\n",
					msg[count]->msg);
#endif
				fprintf(data->output, "%d %s\n",
					UH_INFO_MSG, msg[count]->msg);
				break;
			case PAM_ERROR_MSG:
				/* Error message strings are output verbatim. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: sending error = \"%s\".\n",
					msg[count]->msg);
#endif
				fprintf(data->output, "%d %s\n",
					UH_ERROR_MSG, msg[count]->msg);
				break;
			default:
				/* Maybe the consolehelper can figure out what
				 * to do with this, because we sure can't. */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: sending ??? = \"%s\".\n",
					msg[count]->msg);
#endif
				fprintf(data->output, "%d %s\n",
					UH_UNKNOWN_PROMPT, msg[count]->msg);
				break;
		}
	}

	/* Tell the consolehelper how many messages for which we expect to
	 * receive responses. */
#ifdef DEBUG_USERHELPER
	g_print("userhelper: sending expected response count = %d.\n",
		expected_responses);
#endif
	fprintf(data->output, "%d %d\n", UH_EXPECT_RESP, expected_responses);

	/* Tell the consolehelper that we're ready for it to do its thing. */
#ifdef DEBUG_USERHELPER
	g_print("userhelper: sending sync point.\n");
#endif
	fprintf(data->output, "%d\n", UH_SYNC_POINT);
	fflush(NULL);

	/* Now, for the second pass, allocate space for the responses and read
	 * the answers back. */
	reply = g_malloc0((num_msg + 1) * sizeof(struct pam_response));
	data->fallback_chosen = FALSE;

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
	count = received_responses = 0;
	do {
		string = read_reply(data->input);

		/* If we got nothing, and we expected data, then we're done. */
		if ((string == NULL) &&
		    (received_responses < expected_responses)) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: got %d responses, expected %d\n",
				received_responses, expected_responses);
#endif
			data->canceled = TRUE;
			g_free(reply);
			return PAM_ABORT;
		}

#ifdef DEBUG_USERHELPER
		g_print("userhelper: received string type %d, text \"%s\".\n",
			string[0], string[0] ? string + 1 : "");
#endif

		/* If we hit a sync point, we're done. */
		if (string[0] == UH_SYNC_POINT) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: received sync point\n");
#endif
			if (received_responses != expected_responses) {
				/* Whoa, not done yet! */
#ifdef DEBUG_USERHELPER
				g_print("userhelper: got %d responses, "
					"expected %d\n", received_responses,
					expected_responses);
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
			if (data->sn_id) {
				g_free(data->sn_id);
			}
			data->sn_id = string + 1;
			while ((data->sn_id[0] != '\0') &&
			       (g_ascii_isspace(data->sn_id[0]))) {
				data->sn_id++;
			}
			data->sn_id = g_strdup(data->sn_id);
			g_free(string);
#ifdef DEBUG_USERHELPER
			g_print("userhelper: startup id \"%s\"\n", data->sn_id);
#endif
			continue;
		}
#endif

		/* If the user chose to abort, do so. */
		if (string[0] == UH_CANCEL) {
			data->canceled = TRUE;
			g_free(string);
			g_free(reply);
#ifdef DEBUG_USERHELPER
			g_print("userhelper: canceling with PAM_ABORT (%d)\n",
				PAM_ABORT);
#endif
			return PAM_ABORT;
		}

		/* If the user chose to fallback, do so. */
		if (string[0] == UH_FALLBACK) {
			data->fallback_chosen = TRUE;
			g_free(string);
			g_free(reply);
#ifdef DEBUG_USERHELPER
			g_print("userhelper: falling back\n");
#endif
			return PAM_ABORT;
		}

		/* Find the first unanswered prompt. */
		while ((count < num_msg) &&
		       (msg[count]->msg_style != PAM_PROMPT_ECHO_ON) &&
		       (msg[count]->msg_style != PAM_PROMPT_ECHO_OFF)) {
			count++;
		}
		if (count >= num_msg) {
			/* Whoa, TMI! */
#ifdef DEBUG_USERHELPER
			g_print("userhelper: got %d responses, expected < %d\n",
				received_responses, num_msg);
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
		received_responses++;
	} while(TRUE);

	/* Check that we got exactly the number of responses we were
	 * expecting. */
	if (received_responses != expected_responses) {
		/* Must be an error of some sort... */
#ifdef DEBUG_USERHELPER
		g_print("userhelper: got %d responses, expected %d\n",
			received_responses, expected_responses);
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
	const char *service = NULL, *user, *codeset;
	char *text;
	struct app_data *data = appdata_ptr;
	struct pam_message **messages;
	int i, ret;

	codeset = "C";
	g_get_charset(&codeset);
	bind_textdomain_codeset(PACKAGE, codeset);

	get_pam_string_item(data->pamh, PAM_SERVICE, &service);
	user = NULL;
	get_pam_string_item(data->pamh, PAM_USER, &user);

	if (banner == 0) {
		if ((data->banner != NULL) && (data->domain != NULL)) {
			text = g_strdup_printf(dgettext(data->domain, data->banner));
		} else {
			if ((service != NULL) && (strlen(service) > 0)) {
				if (data->fallback_allowed) {
					text = g_strdup_printf(_("You are attempting to run \"%s\" which may benefit from administrative\nprivileges, but more information is needed in order to do so."), service);
				} else {
					text = g_strdup_printf(_("You are attempting to run \"%s\" which requires administrative\nprivileges, but more information is needed in order to do so."), service);
				}
			} else {
				if (data->fallback_allowed) {
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

static void
pipe_conv_exec_start(const struct pam_conv *conv)
{
	struct app_data *data;
	if (conv->conv == converse_pipe) {
		data = conv->appdata_ptr;
		converse_pipe(0, NULL, NULL, data);
		fprintf(data->output, "%d\n", UH_EXEC_START);
		fprintf(data->output, "%d\n", UH_SYNC_POINT);
		fflush(data->output);
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
	struct app_data *data;
	if (conv->conv == converse_pipe) {
		data = conv->appdata_ptr;
#ifdef DEBUG_USERHELPER
		g_print("userhelper: exec failed\n");
#endif
		fprintf(data->output, "%d\n", UH_EXEC_FAILED);
		fprintf(data->output, "%d\n", UH_SYNC_POINT);
		fflush(data->output);
	}
}

/* checks if username is a member of groupname */
static gboolean
is_group_member(const char *username, const char * groupname)
{
	char **mem;
	struct group *gr = getgrnam(groupname);
	struct passwd *pw = getpwnam(username);
	
	if (pw !=NULL && gr != NULL) {
		if (gr->gr_gid == pw->pw_gid) return TRUE;
		for (mem = gr->gr_mem; *mem != NULL; mem++) {
			if (strcmp(*mem, username) == 0) return TRUE;
		}
	}
	return FALSE;
} 

/* checks if username is a member of any of a comma-separated list of groups */
static gboolean
is_grouplist_member(const char *username, const char * grouplist)
{
	char **grouparray;
	gboolean retval = FALSE;
	
	if (grouplist != NULL) {
		grouparray = g_strsplit(grouplist, ",", -1);
		int i;
		for (i = 0; grouparray[i] != NULL; i++) {
			g_strstrip(grouparray[i]);
			if (is_group_member(username, grouparray[i])) {
				retval = TRUE;
				break;
			}
		}
		g_strfreev(grouparray);
	}
	
	return retval;
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
become_normal(const char *user)
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

/* Determine the name of the user who ran userhelper.  For SELinux, this is the
 * user from the previous context, but for everyone else, it's the user under
 * whose ruid we're running in. */
static char *
get_invoking_user(void)
{
	struct passwd *pwd;
	char *ret=NULL;

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

#ifdef DEBUG_USERHELPER
	g_print("userhelper: ruid user = '%s'\n", ret);
#endif

	return ret;
}

/* Determine the name of the user as whom we must authenticate. */
static char *
get_user_for_auth(shvarFile *s)
{
	char *ret;
	char *invoking_user, *configured_user, *configured_asusergroups;

	invoking_user = get_invoking_user();

	ret = NULL;

	if (ret == NULL) {
		/* Determine who we should authenticate as.  If not specified,
		 * or if "<user>" is specified, or if UGROUPS is set and the
		 * invoking user is a member, we authenticate as the invoking
		 * user, otherwise we authenticate as the specified user (which
		 * is usually root, but could conceivably be someone else). */
		configured_user = svGetValue(s, "USER");
		configured_asusergroups = svGetValue(s, "UGROUPS");
		if (configured_user == NULL) {
			ret = invoking_user;
		} else
		if (strcmp(configured_user, "<user>") == 0) {
			free(configured_user);
			ret = invoking_user;
		} else if (configured_asusergroups != NULL) {
			if (is_grouplist_member(invoking_user, configured_asusergroups)) {
				free(configured_user);
				ret = invoking_user;
			} else {
				ret = configured_user;
			}
			free(configured_asusergroups);
		} else if (strcmp(configured_user, "<none>") == 0) {
			exit(ERR_NO_RIGHTS);
		} else {
			ret = configured_user;
		}
	}

	if (ret != NULL) {
		if (invoking_user != ret) {
			free(invoking_user);
		}
#ifdef DEBUG_USERHELPER
		g_print("userhelper: user for auth = '%s'\n", ret);
#endif
		return ret;
	}

#ifdef DEBUG_USERHELPER
	g_print("userhelper: user for auth not known\n");
#endif
	return NULL;
}

/* Change the user's password using the indicated conversation function and
 * application data (which includes the ability to cancel if the user requests
 * it.  For this task, we don't retry on failure. */
static void
passwd(const char *user, struct pam_conv *conv)
{
	int retval;
	struct app_data *data;

	data = conv->appdata_ptr;
	retval = pam_start("passwd", user, conv, &data->pamh);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_start() failed\n");
#endif
		fail_exit(conv->appdata_ptr, retval);
	}

	retval = pam_set_item(data->pamh, PAM_RUSER, user);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_set_item(PAM_RUSER) failed\n");
#endif
		fail_exit(conv->appdata_ptr, retval);
	}

#ifdef DEBUG_USERHELPER
	g_print("userhelper: changing password for \"%s\"\n", user);
#endif
	retval = pam_chauthtok(data->pamh, 0);
#ifdef DEBUG_USERHELPER
	g_print("userhelper: PAM retval = %d (%s)\n", retval,
		pam_strerror(data->pamh, retval));
#endif

	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_chauthtok() failed\n");
#endif
		fail_exit(conv->appdata_ptr, retval);
	}

	retval = pam_end(data->pamh, PAM_SUCCESS);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_end() failed\n");
#endif
		fail_exit(conv->appdata_ptr, retval);
	}
	exit(0);
}

static char *
construct_cmdline(const char *argv0, char **argv)
{
	int i;
	char *ret, *tmp;
	ret = g_strdup(argv0);
	if ((argv != NULL) && (argv[0] != NULL)) {
		for (i = 1; argv[i] != NULL; i++) {
			tmp = g_strconcat(ret, " ", argv[i], NULL);
			g_free(ret);
			ret = tmp;
		}
	}
	return ret;
}

static void
wrap(const char *user, const char *program,
     struct pam_conv *conv, struct pam_conv *text_conv,
     int argc, char **argv)
{
	/* We're here to wrap the named program.  After authenticating as the
	 * user given in the console.apps configuration file, execute the
	 * command given in the console.apps file. */
	char *constructed_path;
	char *apps_filename;
	char *user_pam;
	const char *auth_user;
	char *apps_banner;
#ifdef USE_STARTUP_NOTIFICATION
	char *apps_sn;
#endif
	const char *apps_domain;
	char *retry, *noxoption;
	char **environ_save;
	char *env_home, *env_term, *env_desktop_startup_id;
	char *env_display, *env_shell;
	char *env_lang, *env_lcall, *env_lcmsgs, *env_xauthority;
	int session, tryagain, gui, retval;
	struct stat sbuf;
	struct passwd *pwd;
	struct app_data *data;
	shvarFile *s;

	/* Find the basename of the command we're wrapping. */
	if (strrchr(program, '/')) {
		program = strrchr(program, '/') + 1;
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
					program);
	s = svNewFile(apps_filename);

	/* If the file is world-writable, or isn't a regular file, or couldn't
	 * be opened, just exit.  We don't want to alert an attacker that the
	 * service name is invalid. */
	if ((s == NULL) ||
	    (fstat(s->fd, &sbuf) == -1) ||
	    !S_ISREG(sbuf.st_mode) ||
	    (sbuf.st_mode & S_IWOTH)) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: bad file permissions: %s \n", apps_filename);
#endif
		exit(ERR_UNK_ERROR);
	}

	data = conv->appdata_ptr;
	user_pam = get_user_for_auth(s);

#ifdef WITH_SELINUX
	if (selinux_enabled) {
		selinux_init(s);
	}
#endif

	/* Read the path to the program to run. */
	constructed_path = svGetValue(s, "PROGRAM");
	if (!constructed_path || constructed_path[0] != '/') {
		/* Criminy....  The system administrator didn't give us an
		 * absolute path to the program!  Guess either /usr/sbin or
		 * /sbin, and then give up if we don't find anything by that
		 * name in either of those directories.  FIXME: we're a setuid
		 * app, so access() may not be correct here, as it may give
		 * false negatives.  But then, it wasn't an absolute path. */
		constructed_path = g_strdup_printf("/usr/sbin/%s", program);
		if (access(constructed_path, X_OK) != 0) {
			/* Try the second directory. */
			strcpy(constructed_path, "/sbin/");
			strcat(constructed_path, program);
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
		conv = text_conv;
	}

	/* We can use a magic configuration file option to disable
	 * the GUI, too. */
	if (gui) {
		noxoption = svGetValue(s, "NOXOPTION");
		if (noxoption && (strlen(noxoption) > 1)) {
			int i;
			for (i = optind; i < argc; i++) {
				if (strcmp(argv[i], noxoption) == 0) {
					conv = text_conv;
					break;
				}
			}
		}
	}

	/* Verify that the user we need to authenticate as has a home
	 * directory. */
	pwd = getpwnam(user_pam);
	if (pwd == NULL) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: no user named %s exists\n", user_pam);
#endif
		exit(ERR_NO_USER);
	}

	/* If the user we're authenticating as has root's UID, then it's
	 * safe to let them use HOME=~root. */
	if (pwd->pw_uid == 0) {
		setenv("HOME", g_strdup(pwd->pw_dir), 1);
	} else {
		/* Otherwise, if they had a reasonable value for HOME, let them
		 * use it. */
		if (env_home != NULL) {
			setenv("HOME", env_home, 1);
		} else {
			/* Otherwise, set HOME to the user's home directory. */
			pwd = getpwuid(getuid());
			if ((pwd != NULL) && (pwd->pw_dir != NULL)) {
				setenv("HOME", g_strdup(pwd->pw_dir), 1);
			}
		}
	}

	/* Read other settings. */
	session = svTrueValue(s, "SESSION", FALSE);
	data->fallback_allowed = svTrueValue(s, "FALLBACK", FALSE);
	retry = svGetValue(s, "RETRY"); /* default value is "2" */
	tryagain = retry ? atoi(retry) + 1 : 3;

	/* Read any custom messages we might want to use. */
	apps_banner = svGetValue(s, "BANNER");
	if ((apps_banner != NULL) && (strlen(apps_banner) > 0)) {
		data->banner = apps_banner;
	}
	apps_domain = svGetValue(s, "DOMAIN");
	if ((apps_domain != NULL) && (strlen(apps_domain) > 0)) {
		bindtextdomain(apps_domain, DATADIR "/locale");
		bind_textdomain_codeset(apps_domain, "UTF-8");
		data->domain = apps_domain;
	}
	if (data->domain == NULL) {
		apps_domain = svGetValue(s, "BANNER_DOMAIN");
		if ((apps_domain != NULL) &&
		    (strlen(apps_domain) > 0)) {
			bindtextdomain(apps_domain, DATADIR "/locale");
			bind_textdomain_codeset(apps_domain, "UTF-8");
			data->domain = apps_domain;
		}
	}
	if (data->domain == NULL) {
		data->domain = program;
	}
#ifdef USE_STARTUP_NOTIFICATION
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_NAME");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		data->sn_name = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_DESCRIPTION");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		data->sn_description = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_WMCLASS");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		data->sn_wmclass = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_BINARY_NAME");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		data->sn_binary_name = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_ICON_NAME");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		data->sn_icon_name = apps_sn;
	}
	apps_sn = svGetValue(s, "STARTUP_NOTIFICATION_WORKSPACE");
	if ((apps_sn != NULL) && (strlen(apps_sn) > 0)) {
		data->sn_workspace = atoi(apps_sn);
	}
#endif

	/* Now we're done reading the file. Close it. */
	svCloseFile(s);

	/* Start up PAM to authenticate the specified user. */
	retval = pam_start(program, user_pam, conv, &data->pamh);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_start() failed\n");
#endif
		fail_exit(conv->appdata_ptr, retval);
	}

	/* Set the requesting user. */
	retval = pam_set_item(data->pamh, PAM_RUSER, user);
	if (retval != PAM_SUCCESS) {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: pam_set_item(PAM_RUSER) failed\n");
#endif
		fail_exit(conv->appdata_ptr, retval);
	}

	/* Try to authenticate the user. */
	do {
#ifdef DEBUG_USERHELPER
		g_print("userhelper: authenticating \"%s\"\n",
			user_pam);
#endif
		retval = pam_authenticate(data->pamh, 0);
#ifdef DEBUG_USERHELPER
		g_print("userhelper: PAM retval = %d (%s)\n", retval,
			pam_strerror(data->pamh, retval));
#endif
		tryagain--;
	} while ((retval != PAM_SUCCESS) && tryagain &&
		 !data->fallback_chosen && !data->canceled);

	if (retval != PAM_SUCCESS) {
		pam_end(data->pamh, retval);
		if (data->canceled) {
			fail_exit(conv->appdata_ptr, retval);
		} else
		if (data->fallback_allowed) {
			/* Reset the user's environment so that the
			 * application can run normally. */
			argv[optind - 1] = strdup(program);
			environ = environ_save;
			become_normal(user);
			if (data->input != NULL) {
				fflush(data->input);
				fcntl(UH_INFILENO, F_SETFD, FD_CLOEXEC);
			}
			if (data->output != NULL) {
				fflush(data->output);
				fcntl(UH_OUTFILENO, F_SETFD, FD_CLOEXEC);
			}
			pipe_conv_exec_start(conv);
#ifdef USE_STARTUP_NOTIFICATION
			if (data->sn_id) {
#ifdef DEBUG_USERHELPER
				g_print("userhelper: setting "
					"DESKTOP_STARTUP_ID =\"%s\"\n",
					data->sn_id);
#endif
				setenv("DESKTOP_STARTUP_ID",
				       data->sn_id, 1);
			}
#endif
#ifdef WITH_SELINUX
			if (selinux_enabled) {
				if (getprevcon(&new_context) < 0) {
					syslog(LOG_NOTICE,"Unable to retrieve SELinux security context\n");
					if (security_getenforce()>0) 
						exit(ERR_EXEC_FAILED);
				}
				if (setup_selinux_exec(constructed_path)) {
					exit(ERR_EXEC_FAILED);
				}
			}
#endif
			execv(constructed_path, argv + optind - 1);
			pipe_conv_exec_fail(conv);
			exit(ERR_EXEC_FAILED);
		} else {
			/* Well, we tried. */
			fail_exit(conv->appdata_ptr, retval);
		}
	}

	/* Verify that the authenticated user is the user we started
	 * out trying to authenticate. */
	retval = get_pam_string_item(data->pamh, PAM_USER, &auth_user);
	if (retval != PAM_SUCCESS) {
		pam_end(data->pamh, retval);
		fail_exit(conv->appdata_ptr, retval);
	}
	if (strcmp(user_pam, auth_user) != 0) {
		exit(ERR_UNK_ERROR);
	}

	/* Verify that the authenticated user is allowed to run this
	 * service now. */
	retval = pam_acct_mgmt(data->pamh, 0);
	if (retval != PAM_SUCCESS) {
		pam_end(data->pamh, retval);
		fail_exit(conv->appdata_ptr, retval);
	}

	/* We need to re-read the user's information -- libpam doesn't
	 * guarantee that these won't be nuked. */
	pwd = getpwnam(user_pam);
	if (pwd == NULL) {
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
		retval = pam_open_session(data->pamh, 0);
		if (retval != PAM_SUCCESS) {
			pam_end(data->pamh, retval);
			fail_exit(conv->appdata_ptr, retval);
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
			const char *cmdline;

			env_pam = pam_getenvlist(data->pamh);
			while (env_pam && *env_pam) {
#ifdef DEBUG_USERHELPER
				g_print("userhelper: setting %s\n",
					*env_pam);
#endif
				putenv(g_strdup(*env_pam));
				env_pam++;
			}

			argv[optind - 1] = strdup(program);
#ifdef DEBUG_USERHELPER
			g_print("userhelper: about to exec \"%s\"\n",
				constructed_path);
#endif
			become_super();
			if (data->input != NULL) {
				fflush(data->input);
				fcntl(UH_INFILENO, F_SETFD, FD_CLOEXEC);
			}
			if (data->output != NULL) {
				fflush(data->output);
				fcntl(UH_OUTFILENO, F_SETFD, FD_CLOEXEC);
			}
			pipe_conv_exec_start(conv);
#ifdef USE_STARTUP_NOTIFICATION
			if (data->sn_id) {
#ifdef DEBUG_USERHELPER
				g_print("userhelper: setting "
					"DESKTOP_STARTUP_ID =\"%s\"\n",
					data->sn_id);
#endif
				setenv("DESKTOP_STARTUP_ID",
				       data->sn_id, 1);
			}
#endif
#ifdef WITH_SELINUX
			if (setup_selinux_exec(constructed_path)) {
			  exit(ERR_EXEC_FAILED);
			}
#endif
			cmdline = construct_cmdline(constructed_path,
						    argv + optind - 1);
#ifdef DEBUG_USERHELPER
			g_print("userhelper: running '%s' with "
				"root privileges on behalf of '%s'.\n",
				cmdline, user);
#endif
			syslog(LOG_NOTICE, "running '%s' with "
			       "root privileges on behalf of '%s'",
			       cmdline, user);
			execv(constructed_path, argv + optind - 1);
			syslog(LOG_ERR, "could not run '%s' with "
			       "root privileges on behalf of '%s': %s",
			       cmdline, user, strerror(errno));
			pipe_conv_exec_fail(conv);
			exit(ERR_EXEC_FAILED);
		}
		/* We're in the parent.  Wait for the child to exit. */
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		wait4(child, &status, 0, NULL);

		/* Close the session. */
		retval = pam_close_session(data->pamh, 0);
		if (retval != PAM_SUCCESS) {
			pam_end(data->pamh, retval);
			fail_exit(conv->appdata_ptr, retval);
		}

		/* Use the exit status fo the child to determine our
		 * exit value. */
		if (WIFEXITED(status)) {
			pam_end(data->pamh, PAM_SUCCESS);
			retval = 0;
		} else {
			pam_end(data->pamh, PAM_SUCCESS);
			retval = ERR_UNK_ERROR;
		}
		exit(retval);
	} else {
		const char *cmdline;

		/* We're not opening a session, so we can just exec()
		 * the program we're wrapping. */
		pam_end(data->pamh, PAM_SUCCESS);

		argv[optind - 1] = strdup(program);
#ifdef DEBUG_USERHELPER
		g_print("userhelper: about to exec \"%s\"\n",
			constructed_path);
#endif
		become_super();
		if (data->input != NULL) {
			fflush(data->input);
			fcntl(UH_INFILENO, F_SETFD, FD_CLOEXEC);
		}
		if (data->output != NULL) {
			fflush(data->output);
			fcntl(UH_OUTFILENO, F_SETFD, FD_CLOEXEC);
		}
		pipe_conv_exec_start(conv);
#ifdef USE_STARTUP_NOTIFICATION
		if (data->sn_id) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: setting "
				"DESKTOP_STARTUP_ID =\"%s\"\n",
				data->sn_id);
#endif
			setenv("DESKTOP_STARTUP_ID", data->sn_id, 1);
		}
#endif
#ifdef WITH_SELINUX
		if (setup_selinux_exec(constructed_path)) {
		  exit(ERR_EXEC_FAILED);
		}
#endif
		cmdline = construct_cmdline(constructed_path,
					    argv + optind - 1);
#ifdef WITH_SELINUX
		if (new_context != NULL) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: running '%s' with root privileges "
				"in context '%s' on behalf of '%s'\n", cmdline,
				new_context, user);
#endif
			syslog(LOG_NOTICE, "running '%s' with root privileges "
			       "in '%s' context on behalf of '%s'", cmdline,
			       new_context, user);
		} else {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: running '%s' with root privileges "
				"on behalf of '%s'\n", cmdline, user);
#endif
			syslog(LOG_NOTICE, "running '%s' with "
			       "root privileges on behalf of '%s'",
			       cmdline, user);
		}
#else
#ifdef DEBUG_USERHELPER
		g_print("userhelper: running '%s' with root privileges "
			"on behalf of '%s'\n", cmdline, user);
#endif
		syslog(LOG_NOTICE, "running '%s' with "
		       "root privileges on behalf of '%s'",
		       cmdline, user);
#endif
		execv(constructed_path, argv + optind - 1);
		syslog(LOG_ERR, "could not run '%s' with "
		       "root privileges on behalf of '%s': %s",
		       cmdline, user, strerror(errno));
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
	char *wrapped_program = NULL;
	char *user_name; /* current user, as determined by real uid */
	int c_flag;	 /* -c flag = change password */
	int t_flag;	 /* -t flag = direct interactive text-mode -- exec'ed */
	int w_flag;	 /* -w flag = act as a wrapper for next * args */
#ifdef WITH_SELINUX
	unsigned perm;
#endif

	/* State variable we pass around. */
	struct app_data app_data = {
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

	/* PAM conversation structures containing the addresses of the
	 * various conversation functions and our state data. */
	struct pam_conv silent_conv = {
		silent_converse,
		&app_data,
	};
	struct pam_conv pipe_conv = {
		converse_pipe,
		&app_data,
	};
	struct pam_conv text_conv = {
		converse_console,
		&app_data,
	};
	struct pam_conv *conv;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, DATADIR "/locale");
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);
	openlog("userhelper", LOG_PID, LOG_AUTHPRIV);

#ifdef WITH_SELINUX
	selinux_enabled = (is_selinux_enabled() > 0);
#endif

	if (geteuid() != 0) {
		fprintf(stderr, _("userhelper must be setuid root\n"));
#ifdef DEBUG_USERHELPER
		g_print("userhelper: not setuid\n");
#endif
		exit(ERR_NO_RIGHTS);
	}

	c_flag = 0;
	t_flag = 0;
	w_flag = 0;

	while ((w_flag == 0) &&
	       (arg = getopt(argc, argv, "ctw:")) != -1) {
		/* We process no arguments after -w program; those are passed
		 * on to a wrapped program. */
		switch (arg) {
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
	if ((c_flag && w_flag) ||
	    (!c_flag && !w_flag)) {
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
	}

	user_name = get_invoking_user();
#ifdef DEBUG_USERHELPER
	g_print("userhelper: current user is %s\n", user_name);
#endif

	/* Change password? */
	if (c_flag) {
		struct passwd *pwd;

		/* the last argument can be a user's name. */
		if ((getuid() == 0) && (argc == optind + 1)) {
			/* We were started by the superuser, so accept the
			 * user's name. */
			user_name = g_strdup(argv[optind]);

#ifdef WITH_SELINUX
			if (selinux_enabled && 
			    checkAccess(PASSWD__PASSWD) != 0) {
				security_context_t context = NULL;
				getprevcon(&context);
				syslog(LOG_NOTICE, 
				       "SELinux context %s is not allowed to change information for user \"%s\"\n",
				       context, user_name);
				g_free(user_name);
				exit(ERR_NO_USER);
			}
#endif
#ifdef DEBUG_USERHELPER
			g_print("userhelper: modifying account data for %s\n",
				user_name);
#endif
		}

		/* Verify that the user exists. */
		pwd = getpwnam(user_name);
		if ((pwd == NULL) || (pwd->pw_name == NULL)) {
#ifdef DEBUG_USERHELPER
			g_print("userhelper: user %s doesn't exist\n",
				user_name);
#endif
			exit(ERR_NO_USER);
		}

		passwd(user_name, conv);
		g_assert_not_reached();
	}

	/* Wrap some other program? */
	if (w_flag) {
		wrap(user_name, wrapped_program, conv, &text_conv,
		     argc, argv);
		g_assert_not_reached();
	}

	/* Not reached. */
	g_assert_not_reached();
	exit(0);
}
