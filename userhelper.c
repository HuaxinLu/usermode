/*
 * Copyright (C) 1997-2003, 2007, 2008 Red Hat, Inc.  All rights reserved.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include "config.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libintl.h>
#include <locale.h>
#include <math.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include <libuser/user.h>

#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#endif

#include "shvar.h"
#include "userhelper.h"
#include "userhelper-messages.h"

#ifdef DEBUG_USERHELPER
#define debug_msg(...) g_print(__VA_ARGS__)
#else
#define debug_msg(...) ((void)0)
#endif

/* Execute permissions. */
#ifdef HAVE_FEXECVE
#  define UH_S_IXALL (S_IXUSR | S_IXGRP | S_IXOTH)
#endif

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

/* A structure type which we use to carry psuedo-global data around with us. */
struct app_data {
	const struct pam_conv *conv;
	pam_handle_t *pamh;
	gboolean fallback_allowed, fallback_chosen, canceled;
	FILE *input, *output;
	const char *banner, *domain;
	const char *sn_name, *sn_description, *sn_wmclass;
	const char *sn_binary_name, *sn_icon_name;
	char *sn_id;
	int sn_workspace;
};

/* A mixed-mode conversation function suitable for use with X.
   data->conv->conv == converse_pipe is used to check whether we are run
   by userhelper_runv() which e.g. writes error messages for us. */
static int converse_pipe(int num_msg, const struct pam_message **msg,
			 struct pam_response **resp, void *appdata_ptr);

#ifdef WITH_SELINUX
static int checkAccess(unsigned int selaccess) {
  int status=-1;
  security_context_t user_context;
  if( getprevcon(&user_context)==0 ) {
    struct av_decision avd;
    int retval = security_compute_av(user_context,
				     user_context,
				     string_to_security_class("passwd"),
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
#endif /* WITH_SELINUX */

/* Exit, optionally writing a message to stderr. */
static void G_GNUC_NORETURN
die(struct app_data *data, int status)
{
	/* Be silent when status == 0, UNIX convention is to only report
	   errors. */
	if (status != 0 && data->conv->conv != converse_pipe) {
		const char *msg;
		enum uh_message_type type;

		uh_exitstatus_message(status, &msg, &type);
		if (type != UHM_SILENT)
			fprintf(stderr, "%s\n", msg);
	}
	exit(status);
}

/* Exit, returning the proper status code based on a PAM error code.
   Optionally write a message to stderr. */
static void G_GNUC_NORETURN
fail_exit(struct app_data *data, int pam_retval)
{
	int status;

	/* This is a local error.  Bail. */
	if (pam_retval == ERR_SHELL_INVALID)
		die(data, ERR_SHELL_INVALID);

	if (pam_retval == PAM_SUCCESS)
		/* Just exit. */
		status = 0;
	/* Map the PAM error code to a local error code and return it to the
	   parent process.  Trust the canceled flag before any PAM error
	   codes. */
	else if (data->canceled)
		status = ERR_CANCELED;
	else {
		debug_msg("userhelper: got PAM error %d.\n", pam_retval);
		switch (pam_retval) {
			case PAM_OPEN_ERR:
			case PAM_SYMBOL_ERR:
			case PAM_SERVICE_ERR:
			case PAM_SYSTEM_ERR:
			case PAM_BUF_ERR:
				status = ERR_PAM_INT_ERROR;
				break;
			case PAM_AUTH_ERR:
			case PAM_AUTHTOK_ERR:
			case PAM_PERM_DENIED:
				status = ERR_PASSWD_INVALID;
				break;
			case PAM_AUTHTOK_LOCK_BUSY:
				status = ERR_LOCKS;
				break;
			case PAM_CRED_INSUFFICIENT:
			case PAM_AUTHINFO_UNAVAIL:
			case PAM_CRED_UNAVAIL:
			case PAM_CRED_EXPIRED:
			case PAM_AUTHTOK_EXPIRED:
				status = ERR_NO_RIGHTS;
				break;
			case PAM_USER_UNKNOWN:
				status = ERR_NO_USER;
				break;
			case PAM_MAXTRIES:
				status = ERR_MAX_TRIES;
				break;
			case PAM_ABORT:
				/* fall through */
			default:
				status = ERR_UNK_ERROR;
				break;
		}
	}
	debug_msg("userhelper: exiting with status %d.\n", status);
	die(data, status);
}

/* Read a string from stdin, and return a freshly-allocated copy, without
 * the end-of-line terminator if there was one, and with an optional
 * consolehelper message header removed. */
static char *
read_reply(FILE *fp)
{
	char buffer[BUFSIZ];
	size_t slen;

	if (feof(fp))
		return NULL;
	if (fgets(buffer, sizeof(buffer), fp) == NULL)
		return NULL;

	slen = strlen(buffer);
	while (slen > 1
	       && (buffer[slen - 1] == '\n' || buffer[slen - 1] == '\r')) {
		buffer[slen - 1] = '\0';
		slen--;
	}

	return g_strdup(buffer);
}

/* Send a request. */
static void
send_request(FILE *fp, char request_type, const char *data)
{
	size_t len;

	if (data == NULL)
		data = "";
	len = strlen(data);
	assert(len < powl(10.0, UH_REQUEST_SIZE_DIGITS));
	fprintf(fp, "%c%0*zu%s\n", request_type, UH_REQUEST_SIZE_DIGITS, len,
		data);
}

/* Send a request with an integer payload. */
static void
send_request_int(FILE *fp, char request_type, int data)
{
	/* log2(10) > 3, so sizeof(int) * CHAR_BIT / 3 digits are necessary
	   to represent all values, + 1 to avoid rounding down "partial
	   digits". */
	char buf[sizeof(int) * CHAR_BIT / 3 + 1 + 1];

	sprintf(buf, "%d", data);
	send_request(fp, request_type, buf);
}

/* A text-mode conversation function suitable for use when there is no
 * controlling terminal. */
static int
silent_converse(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *appdata_ptr)
{
	(void)num_msg;
	(void)msg;
	(void)resp;
	(void)appdata_ptr;
	return PAM_CONV_ERR;
}

static int
get_pam_string_item(pam_handle_t *pamh, int item, const char **out)
{
	const void *s;
	int ret;

	ret = pam_get_item(pamh, item, &s);
	*out = s;
	return ret;
}

/* Free the first COUNT entries of RESP, and RESP itself */
static void
free_reply(struct pam_response *resp, size_t count)
{
	size_t i;

	for (i = 0; i < count; i++)
		free(resp[i].resp);
	free(resp);
}

/* Send startup notification-related information using DATA */
static void
send_sn_info(struct app_data *data)
{
	(void)data;
#ifdef USE_STARTUP_NOTIFICATION
#define S(OP, MEMBER, DESCRIPTION)					\
	do {								\
		if (data->domain != NULL && data->MEMBER != NULL) {	\
			debug_msg("userhelper (cp): sending " DESCRIPTION \
				  " `%s'\n", data->MEMBER);		\
			send_request(data->output, OP,			\
				     dgettext(data->domain, data->MEMBER)); \
		}							\
	} while (0)
	S(UH_SN_NAME, sn_name, "sn name");
	S(UH_SN_DESCRIPTION, sn_description, "sn description");
	S(UH_SN_WMCLASS, sn_wmclass, "sn wm_class");
	S(UH_SN_BINARY_NAME, sn_binary_name, "sn binary name");
	S(UH_SN_ICON_NAME, sn_icon_name, "sn icon name");
#undef S

	if (data->sn_workspace != -1) {
		debug_msg("userhelper (cp): sending sn workspace %d.\n",
			  data->sn_workspace);
		send_request_int(data->output, UH_SN_WORKSPACE,
				 data->sn_workspace);
	}
#endif
}

/* A mixed-mode conversation function suitable for use with X. */
static int
converse_pipe(int num_msg, const struct pam_message **msg,
	      struct pam_response **resp, void *appdata_ptr)
{
	int count, expected_responses, received_responses;
	struct pam_response *reply;
	char *string;
	const char *user, *service;
	struct app_data *data = appdata_ptr;

	/* Pass on any hints we have to the consolehelper. */

	/* Since PAM does not handle our cancel request we'll we have to do it
	   ourselves.  Don't bother user with messages if already canceled. */
	if (data->canceled) {
		debug_msg("userhelper (cp): we were already canceled\n");
		return PAM_ABORT;
	}


	/* User. */
	if ((get_pam_string_item(data->pamh, PAM_USER, &user) != PAM_SUCCESS) ||
	    (user == NULL) ||
	    (strlen(user) == 0)) {
		user = "root";
	}
	debug_msg("userhelper (cp): converse_pipe_called(num_msg=%d, "
		  "canceled=%d)\n", num_msg, data->canceled);
	debug_msg("userhelper (cp): sending user `%s'\n", user);
	send_request(data->output, UH_USER, user);

	/* Service. */
	if (get_pam_string_item(data->pamh, PAM_SERVICE,
				&service) == PAM_SUCCESS) {
		debug_msg("userhelper (cp): sending service `%s'\n", service);
		send_request(data->output, UH_SERVICE_NAME, service);
	}

	/* Fallback allowed? */
	debug_msg("userhelper (cp): sending fallback = %d.\n",
		  data->fallback_allowed ? 1 : 0);
	send_request_int(data->output, UH_FALLBACK_ALLOW,
			 data->fallback_allowed ? 1 : 0);

	/* Banner. */
	if ((data->domain != NULL) && (data->banner != NULL)) {
		debug_msg("userhelper (cp): sending banner `%s'\n",
			  data->banner);
		send_request(data->output, UH_BANNER,
			     dgettext(data->domain, data->banner));
	}

	send_sn_info(data);

	/* We do a first pass on all items and output them, and then a second
	 * pass to read responses from the helper. */
	for (count = expected_responses = 0; count < num_msg; count++) {
		switch (msg[count]->msg_style) {
			case PAM_PROMPT_ECHO_ON:
				/* Spit out the prompt. */
				debug_msg("userhelper (cp): sending prompt "
					  "(echo on) = \"%s\".\n",
					  msg[count]->msg);
				send_request(data->output, UH_ECHO_ON_PROMPT,
					     msg[count]->msg);
				expected_responses++;
				break;
			case PAM_PROMPT_ECHO_OFF:
				debug_msg("userhelper (cp): sending prompt (no "
					  "echo) = \"%s\".\n", msg[count]->msg);
				send_request(data->output, UH_ECHO_OFF_PROMPT,
					     msg[count]->msg);
				expected_responses++;
				break;
			case PAM_TEXT_INFO:
				/* Text information strings are output
				 * verbatim. */
				debug_msg("userhelper (cp): sending text = "
					  "\"%s\".\n", msg[count]->msg);
				send_request(data->output, UH_INFO_MSG,
					     msg[count]->msg);
				break;
			case PAM_ERROR_MSG:
				/* Error message strings are output verbatim. */
				debug_msg("userhelper (cp): sending error = "
					  "\"%s\".\n", msg[count]->msg);
				send_request(data->output, UH_ERROR_MSG,
					     msg[count]->msg);
				break;
			default:
				/* Maybe the consolehelper can figure out what
				 * to do with this, because we sure can't. */
				debug_msg("userhelper (cp): sending ??? = "
					  "\"%s\".\n", msg[count]->msg);
				send_request(data->output, UH_UNKNOWN_PROMPT,
					     msg[count]->msg);
				break;
		}
	}

	/* Tell the consolehelper how many messages for which we expect to
	 * receive responses. */
	debug_msg("userhelper (cp): sending expected response count = %d.\n",
		  expected_responses);
	send_request_int(data->output, UH_EXPECT_RESP, expected_responses);

	/* Tell the consolehelper that we're ready for it to do its thing. */
	debug_msg("userhelper (cp): sending sync point.\n");
	send_request(data->output, UH_SYNC_POINT, NULL);
	fflush(NULL);

	/* Now, for the second pass, allocate space for the responses and read
	 * the answers back. */
	reply = calloc(num_msg, sizeof(*reply));
	data->fallback_chosen = FALSE;

	/* First, handle the items which don't require answers. */
	for (count = 0; count < num_msg; count++) {
		switch (msg[count]->msg_style) {
		case PAM_TEXT_INFO:
		case PAM_ERROR_MSG:
			/* Ignore it... */
			/* reply[count].resp = NULL; set by the calloc ()
			   above */
			reply[count].resp_retcode = PAM_SUCCESS;
			break;
		default:
			break;
		}
	}

	/* Now read responses until we hit a sync point or an EOF. */
	count = received_responses = 0;
	for (;;) {
		string = read_reply(data->input);

		/* If we got nothing, and we expected data, then we're done. */
		if (string == NULL) {
			debug_msg("userhelper (cp): got %d responses, expected "
				  "%d\n", received_responses,
				  expected_responses);
			data->canceled = TRUE;
			free_reply(reply, count);
			return PAM_ABORT;
		}

		debug_msg("userhelper (cp): received string type %d, text "
			  "\"%s\".\n", string[0], string[0] ? string + 1 : "");

		/* If we hit a sync point, we're done. */
		if (string[0] == UH_SYNC_POINT) {
			debug_msg("userhelper (cp): received sync point\n");
			g_free(string);
			if (data->fallback_chosen) {
				debug_msg("userhelper (cp): falling back\n");
				free_reply(reply, count);
				return PAM_ABORT;
			}
			if (received_responses != expected_responses) {
				/* Whoa, not done yet! */
				debug_msg("userhelper (cp): got %d responses, "
					  "expected %d\n", received_responses,
					  expected_responses);
				free_reply(reply, count);
				return PAM_CONV_ERR;
			}
			/* Okay, we're done. */
			break;
		}

		/* If we got a desktop startup ID, set it. */
		if (string[0] == UH_SN_ID) {
			g_free(data->sn_id);
			data->sn_id = g_strdup(string + 1);
			g_free(string);
			debug_msg("userhelper (cp): startup id \"%s\"\n",
				  data->sn_id);
			continue;
		}

		/* If the user chose to abort, do so. */
		if (string[0] == UH_CANCEL) {
			data->canceled = TRUE;
			g_free(string);
			free_reply(reply, count);
			debug_msg("userhelper (cp): canceling with PAM_ABORT "
				  "(%d)\n", PAM_ABORT);
			return PAM_ABORT;
		}

		/* If the user chose to fallback, do so. */
		if (string[0] == UH_FALLBACK) {
			data->fallback_chosen = TRUE;
			g_free(string);
			debug_msg("userhelper (cp): will fall back\n");
			continue;
		}

		/* Find the first unanswered prompt. */
		while ((count < num_msg) &&
		       (msg[count]->msg_style != PAM_PROMPT_ECHO_ON) &&
		       (msg[count]->msg_style != PAM_PROMPT_ECHO_OFF)) {
			count++;
		}
		if (count >= num_msg) {
			/* Whoa, TMI! */
			debug_msg("userhelper (cp): got %d responses, expected "
				  "< %d\n", received_responses, num_msg);
			g_free(string);
			free_reply(reply, count);
			return PAM_CONV_ERR;
		}

		/* Save this response. */
		reply[count].resp = strdup(string + 1);
		g_free(string);
		if (reply[count].resp == NULL) {
			free_reply(reply, count);
			return PAM_BUF_ERR;
		}
		reply[count].resp_retcode = PAM_SUCCESS;
		debug_msg("userhelper (cp): got `%s'\n", reply[count].resp);
		count++;
		received_responses++;
	}

	/* Check that we got exactly the number of responses we were
	 * expecting. */
	if (received_responses != expected_responses) {
		/* Must be an error of some sort... */
		debug_msg("userhelper (cp): got %d responses, expected %d\n",
			  received_responses, expected_responses);
		free_reply(reply, count);
		return PAM_CONV_ERR;
	}

	/* Return successfully. */
	if (resp != NULL)
		*resp = reply;
	else
		free_reply(reply, count);
	return PAM_SUCCESS;
}

/* A conversation function which wraps the one provided by libpam_misc. */
static int
converse_console(int num_msg, const struct pam_message **msg,
		 struct pam_response **resp, void *appdata_ptr)
{
	static int banner = 0;
	const char *service, *user;
	char *text;
	struct app_data *data = appdata_ptr;
	struct pam_message **messages;
	int i, ret;

	if (get_pam_string_item(data->pamh, PAM_SERVICE, &service)
	    != PAM_SUCCESS)
		service = NULL;
	if (get_pam_string_item(data->pamh, PAM_USER, &user) != PAM_SUCCESS)
		user = NULL;

	if (banner == 0) {
		if ((data->banner != NULL) && (data->domain != NULL)) {
			text = g_strdup (dgettext(data->domain, data->banner));
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
		if (user != NULL && strlen(user) != 0) {
			fprintf(stdout, _("Authenticating as \"%s\""), user);
			putchar('\n');
			fflush(stdout);
		}
		banner++;
	}

	messages = g_malloc_n(num_msg, sizeof(*messages));
	for (i = 0; i < num_msg; i++) {
		messages[i] = g_malloc(sizeof(*(messages[i])));
		*(messages[i]) = *(msg[i]);
		if (msg[i]->msg != NULL)
			messages[i]->msg = _(msg[i]->msg);
	}

	ret = misc_conv(num_msg, (const struct pam_message **)messages,
			resp, appdata_ptr);

	for (i = 0; i < num_msg; i++)
		g_free(messages[i]);
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
	struct app_data *data = callback_data;

	/* Pass on any hints we have to the consolehelper. */
	send_request_int(data->output, UH_FALLBACK_ALLOW,
			 data->fallback_allowed ? 1 : 0);

	/* User. */
	if ((get_pam_string_item(data->pamh, PAM_USER, &user) != PAM_SUCCESS) ||
	    (user == NULL) ||
	    (strlen(user) == 0)) {
		user = "root";
	}
	debug_msg("userhelper: sending user `%s'\n", user);
	send_request(data->output, UH_USER, user);

	/* Service. */
	if (get_pam_string_item(data->pamh, PAM_SERVICE,
				&service) == PAM_SUCCESS) {
		send_request(data->output, UH_SERVICE_NAME, service);
	}

	/* We do first a pass on all items and output them, and then a second
	 * pass to read responses from the helper. */
	for (i = 0; i < prompts_count; i++) {
		/* Spit out the prompt. */
		if (prompts[i].default_value) {
			send_request(data->output, UH_PROMPT_SUGGESTION,
				     prompts[i].default_value);
		}
		send_request(data->output, prompts[i].visible
			     ? UH_ECHO_ON_PROMPT : UH_ECHO_OFF_PROMPT,
			     prompts[i].prompt);
	}

	/* Tell the consolehelper how many messages we expect to get
	 * responses to. */
	send_request_int(data->output, UH_EXPECT_RESP, prompts_count);
	send_request(data->output, UH_SYNC_POINT, NULL);
	fflush(NULL);

	/* Now, for the second pass, allocate space for the responses and read
	 * the answers back. */
	i = 0;
	for (;;) {
		string = read_reply(data->input);

		if (string == NULL) {
			/* EOF: the child isn't going to give us any more
			 * information. */
			data->canceled = TRUE;
			lu_error_new(error, lu_error_generic,
				     "Operation canceled by user");
			goto err_prompts;
		}

		/* If we finished, we're done. */
		if (string[0] == UH_SYNC_POINT) {
			g_free(string);
			if (i < prompts_count) {
				/* Not enough information. */
				debug_msg("userhelper: not enough responses\n");
				lu_error_new(error, lu_error_generic,
					     "Not enough responses returned by "
					     "parent");
				goto err_prompts;
			}
			return TRUE;
		}

		/* If the user chose to abort, do so. */
		if (string[0] == UH_CANCEL) {
			debug_msg("userhelper: user canceled\n");
			g_free(string);
			data->canceled = TRUE;
			lu_error_new(error, lu_error_generic,
				     "Operation canceled by user");
			goto err_prompts;
		}

		/* If the user chose to fallback, do so. */
		if (string[0] == UH_FALLBACK) {
			debug_msg("userhelper: user fell back\n");
			g_free(string);
			data->fallback_chosen = TRUE;
			lu_error_new(error, lu_error_generic,
				     "User has decided to use unprivileged "
				     "mode");
			goto err_prompts;
		}

		/* Save this response. */
		prompts[i].free_value = g_free;
		prompts[i].value = g_strdup(string + 1);
		debug_msg("userhelper: got `%s'\n", prompts[i].value);
		g_free(string);
		i++;
	}

err_prompts:
	while (i != 0) {
		prompts[i].free_value(prompts[i].value);
		i--;
	}
	return FALSE;
}

/* A sync point is expected on the input pipe.  Wait until it arrives. */
static int
pipe_conv_wait_for_sync(struct app_data *data)
{
	char *reply;
	int err;

	if (data->input == NULL) {
		err = ERR_UNK_ERROR;
		goto err;
	}
	reply = read_reply(data->input);
	if (reply == NULL) {
		err = ERR_UNK_ERROR;
		goto err;
	}

	if (reply[0] == UH_SN_ID) {
		/* This happens even outside of converse_pipe; if PAM does not
		   ask any questions at all, this might in fact be the only
		   time UH_SN_ID is sent. */
		g_free(data->sn_id);
		data->sn_id = g_strdup(reply + 1);
		g_free(reply);
		reply = read_reply(data->input);
		if (reply == NULL) {
			err = ERR_UNK_ERROR;
			goto err;
		}
	}

	if (reply[0] != UH_SYNC_POINT) {
		debug_msg("Unexpected reply type %d, \"%s\"\n", reply[0],
			  reply[0] != 0 ? reply + 1 : "");
		err = ERR_UNK_ERROR;
		goto err_reply;
	}
	err = 0;
	/* Fall through */
err_reply:
	g_free(reply);
err:
	return err;
}

static int
pipe_conv_exec_start(struct app_data *data)
{
	if (data->conv->conv == converse_pipe) {
		/* There might have been no converse_pipe() call, so send the
		   information now. */
		if (data->output == NULL)
			return ERR_UNK_ERROR;
		send_sn_info(data);
		send_request(data->output, UH_EXEC_START, NULL);
		send_request(data->output, UH_SYNC_POINT, NULL);
		fflush(data->output);
#ifdef DEBUG_USERHELPER
		{
			int timeout = 5;
			debug_msg("userhelper: exec start\nuserhelper: pausing "
				  "for %d seconds for debugging\n", timeout);
			sleep(timeout);
		}
#endif
		return pipe_conv_wait_for_sync(data);
	}
	return 0;
}

static void
pipe_conv_exec_fail(struct app_data *data)
{
	if (data->conv->conv == converse_pipe) {
		debug_msg("userhelper: exec failed\n");
		/* There might have been no converse_pipe() call, so send the
		   information now, just to be sure. */
		send_sn_info(data);
		send_request(data->output, UH_EXEC_FAILED, NULL);
		send_request(data->output, UH_SYNC_POINT, NULL);
		fflush(data->output);
		/* It is important to keep the parent in sync with our state,
		   even though there is no reliable way to inform it if this
		   fails. */
		(void)pipe_conv_wait_for_sync(data);
	}
}

/* Parse the passed-in GECOS string and set PARSED to its broken-down contents.
   Note that the parsing is performed using the convention obeyed by BSDish
   finger(1) under Linux. */
static void
gecos_parse(const char *gecos, struct gecos_data *parsed)
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

	for (i = 0; exploded[i] != NULL; i++) {
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
		if (dest != NULL) {
			*dest = g_strdup(exploded[i]);
		}
	}

	g_strfreev(exploded);
}

/* A simple function to compute the size of a gecos string containing the
 * data we have. */
static size_t
gecos_size(struct gecos_data *parsed)
{
	size_t len;

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
	size_t i;

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
		ret[i - 1] = '\0';
		i--;
	}
	return ret;
}

/* Free GECOS */
static void
gecos_free(struct gecos_data *gecos)
{
	g_free(gecos->full_name);
	g_free(gecos->office);
	g_free(gecos->office_phone);
	g_free(gecos->home_phone);
	g_free(gecos->site_info);
}

/* Check if the passed-in shell is a valid shell according to getusershell(),
 * which is usually back-ended by /etc/shells.  Treat NULL or the empty string
 * as "/bin/sh", as is traditional. */
static gboolean
shell_valid(const char *shell_name)
{
	gboolean found;

	found = FALSE;
	if (shell_name != NULL) {
		char *shell;

		if (strlen(shell_name) == 0)
			shell_name = "/bin/sh";
		setusershell();
		while ((shell = getusershell()) != NULL) {
			debug_msg("userhelper: got shell \"%s\"\n", shell);
			if (strcmp(shell_name, shell) == 0) {
				found = TRUE;
				break;
			}
		}
		endusershell();
	}
	return found;
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
become_super_supplementary_groups(struct app_data *data)
{
	if (initgroups("root", 0) != 0) {
		debug_msg("userhelper: initgroups() failure: %s\n",
			  strerror(errno));
		die(data, ERR_EXEC_FAILED);
	}
}

static void
become_super_other(struct app_data *data)
{
	/* Become the superuser.
	   Yes, setuid() and friends can fail, even for superusers. */
	if (setregid(0, 0) != 0 || setreuid(0, 0) != 0) {
		debug_msg("userhelper: set*id() failure: %s\n",
			  strerror(errno));
		die(data, ERR_EXEC_FAILED);
	}
	if ((geteuid() != 0) ||
	    (getuid() != 0) ||
	    (getegid() != 0) ||
	    (getgid() != 0)) {
		debug_msg("userhelper: set*id() didn't work\n");
		die(data, ERR_EXEC_FAILED);
	}
}

static void
become_super(struct app_data *data)
{
	become_super_supplementary_groups(data);
	become_super_other(data);
}

static void
become_normal(struct app_data *data, const char *user)
{
	gid_t gid;
	uid_t uid;

	gid = getgid();
	uid = getuid();
	/* Become the user who invoked us. */
	if (initgroups(user, gid) != 0 ||
	    setregid(gid, gid) != 0 ||
	    setreuid(uid, uid) != 0) {
		debug_msg("userhelper: set*id() failure: %s\n",
			  strerror(errno));
		die(data, ERR_EXEC_FAILED);
	}
	/* Verify that we're back to normal. */
	if (getegid() != gid || getgid() != gid) {
		debug_msg("userhelper: still setgid()\n");
		die(data, ERR_EXEC_FAILED);
	}
	/* Yes, setuid() can fail. */
	if (geteuid() != uid || getuid() != uid) {
		debug_msg("userhelper: still setuid()\n");
		die(data, ERR_EXEC_FAILED);
	}
}

/* Determine the name of the user who ran userhelper.
   Exit on error, optionally writing to stderr. */
static char *
get_invoking_user(struct app_data *data)
{
	struct passwd *pwd;
	char *ret=NULL;

	/* Now try to figure out who called us. */
	pwd = getpwuid(getuid());
	if ((pwd != NULL) && (pwd->pw_name != NULL)) {
		ret = g_strdup(pwd->pw_name);
	} else {
		/* I have no name and I must have one. */
		debug_msg("userhelper: i have no name\n");
		die(data, ERR_UNK_ERROR);
	}

	debug_msg("userhelper: ruid user = '%s'\n", ret);

	return ret;
}

/* Determine the name of the user as whom we must authenticate.
   Exit on error, optionally writing to stderr. */
static char *
get_user_for_auth(struct app_data *data, shvarFile *s)
{
	char *ret;
	char *invoking_user, *configured_user, *configured_asusergroups;

	invoking_user = get_invoking_user(data);

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
			g_free(configured_user);
			ret = invoking_user;
		} else if (configured_asusergroups != NULL) {
			if (is_grouplist_member(invoking_user, configured_asusergroups)) {
				g_free(configured_user);
				ret = invoking_user;
			} else
				ret = configured_user;
		} else if (strcmp(configured_user, "<none>") == 0)
			die(data, ERR_NO_RIGHTS);
		else
			ret = configured_user;
		g_free(configured_asusergroups);
	}

	if (ret != NULL) {
		if (invoking_user != ret)
			g_free(invoking_user);
		debug_msg("userhelper: user for auth = '%s'\n", ret);
		return ret;
	}

	debug_msg("userhelper: user for auth not known\n");
	return NULL;
}

/* Set various attributes of DATA, including the requesting user USER.
   Exit on error, optionally writing to stderr. */
static void
set_pam_items(struct app_data *data, const char *user)
{
	int retval;
	char *tty;

	retval = pam_set_item(data->pamh, PAM_RUSER, user);
	if (retval != PAM_SUCCESS) {
		debug_msg("userhelper: pam_set_item(PAM_RUSER) failed\n");
		fail_exit(data, retval);
	}

	tty = ttyname(STDIN_FILENO);
	if (tty != NULL) {
		if (strncmp(tty, "/dev/", 5) == 0)
			tty += 5;
		retval = pam_set_item(data->pamh, PAM_TTY, tty);
		if (retval != PAM_SUCCESS) {
			debug_msg("userhelper: pam_set_item(PAM_TTY) failed\n");
			fail_exit(data, retval);
		}
	}
}

/* Change the user's password using the indicated conversation function and
 * application data (which includes the ability to cancel if the user requests
 * it.  For this task, we don't retry on failure. */
static void G_GNUC_NORETURN
passwd(const char *user, struct app_data *data)
{
	int retval;

	retval = pam_start("passwd", user, data->conv, &data->pamh);
	if (retval != PAM_SUCCESS) {
		debug_msg("userhelper: pam_start() failed\n");
		fail_exit(data, retval);
	}

	set_pam_items(data, user);

	debug_msg("userhelper: changing password for \"%s\"\n", user);
	retval = pam_chauthtok(data->pamh, 0);
	debug_msg("userhelper: PAM retval = %d (%s)\n", retval,
		  pam_strerror(data->pamh, retval));

	if (retval != PAM_SUCCESS) {
		debug_msg("userhelper: pam_chauthtok() failed\n");
		fail_exit(data, retval);
	}

	retval = pam_end(data->pamh, PAM_SUCCESS);
	if (retval != PAM_SUCCESS) {
		debug_msg("userhelper: pam_end() failed\n");
		fail_exit(data, retval);
	}
	exit(0);
}

/* Does STRING contain control characters or characters from FORBIDDEN? */
static gboolean
string_has_forbidden_characters(const char *string, const char *forbidden)
{
	char c;

	if (strpbrk(string, forbidden) != NULL)
		return TRUE;
	for (; (c = *string) != 0; string++) {
		/* This is only a minimal sanity check, primarily to be
		 * consistent with util-linux, not really a security
		 * boundary. */
		if (iscntrl((unsigned char)c))
			return TRUE;
	}

	return FALSE;
}

/* We're here to change the user's non-security information.  PAM doesn't
 * provide an interface to do this, because it's not PAM's job to manage this
 * stuff, so farm it out to a different library. */
static void G_GNUC_NORETURN
chfn(const char *user, struct app_data *data, lu_prompt_fn *prompt,
     const char *new_full_name, const char *new_office,
     const char *new_office_phone, const char *new_home_phone,
     const char *new_shell)
{
	char *new_gecos, *old_gecos, *old_shell;
	struct gecos_data parsed_gecos;
	const char *authed_user;
	struct lu_context *context;
	struct lu_ent *ent;
	struct lu_error *error;
	int tryagain = 3, retval;
	gboolean ret;

	debug_msg("userhelper: chfn(\"%s\", \"%s\", \"%s\", \"%s\", "
		  "\"%s\", \"%s\")\n", user,
		  new_full_name ? new_full_name : "(null)",
		  new_office ? new_office : "(null)",
		  new_office_phone ? new_office_phone : "(null)",
		  new_home_phone ? new_home_phone : "(null)",
		  new_shell ? new_shell : "(null)");

	/* Verify that the fields we were given on the command-line
	 * are sane (i.e., contain no forbidden characters). */
	if (new_full_name != NULL &&
	    string_has_forbidden_characters(new_full_name, ":,=\n"))
		die(data, ERR_FIELDS_INVALID);
	if (new_office != NULL &&
	    string_has_forbidden_characters(new_office, ":,=\n"))
		die(data, ERR_FIELDS_INVALID);
	if (new_office_phone != NULL &&
	    string_has_forbidden_characters(new_office_phone, ":,=\n"))
		die(data, ERR_FIELDS_INVALID);
	if (new_home_phone != NULL &&
	    string_has_forbidden_characters(new_home_phone, ":,=\n"))
		die(data, ERR_FIELDS_INVALID);
	if (new_shell != NULL &&
	    string_has_forbidden_characters(new_shell, ":\n"))
		die(data, ERR_FIELDS_INVALID);

	/* Start up PAM to authenticate the user, this time pretending
	 * we're "chfn". */
	retval = pam_start("chfn", user, data->conv, &data->pamh);
	if (retval != PAM_SUCCESS) {
		debug_msg("userhelper: pam_start() failed\n");
		fail_exit(data, retval);
	}

	set_pam_items(data, user);

	/* Try to authenticate the user. */
	do {
		debug_msg("userhelper: about to authenticate \"%s\"\n", user);
		retval = pam_authenticate(data->pamh, 0);
		debug_msg("userhelper: PAM retval = %d (%s)\n", retval,
			  pam_strerror(data->pamh, retval));
		tryagain--;
	} while ((retval != PAM_SUCCESS) &&
		 (retval != PAM_CONV_ERR) &&
		 !data->canceled &&
		 (tryagain > 0));
	/* If we didn't succeed, bail. */
	if (retval != PAM_SUCCESS) {
		debug_msg("userhelper: pam authentication failed\n");
		pam_end(data->pamh, retval);
		fail_exit(data, retval);
	}

	/* Verify that the authenticated user is the user we started
	 * out trying to authenticate. */
	retval = get_pam_string_item(data->pamh, PAM_USER, &authed_user);
	if (retval != PAM_SUCCESS) {
		debug_msg("userhelper: no pam user set\n");
		pam_end(data->pamh, retval);
		fail_exit(data, retval);
	}

	/* At some point this check will go away. */
	if (strcmp(user, authed_user) != 0) {
		debug_msg("userhelper: username(%s) != authuser(%s)\n",
			  user, authed_user);
		die(data, ERR_UNK_ERROR);
	}

	/* Check if the user is allowed to change her information at
	 * this time, on this machine, yadda, yadda, yadda.... */
	retval = pam_acct_mgmt(data->pamh, 0);
	if (retval != PAM_SUCCESS) {
		debug_msg("userhelper: pam_acct_mgmt() failed\n");
		pam_end(data->pamh, retval);
		fail_exit(data, retval);
	}

	/* Let's get to it.  Start up libuser. */
	error = NULL;
	context = lu_start(user, lu_user, NULL, NULL, prompt, data, &error);
	if (context == NULL) {
		debug_msg("userhelper: libuser startup error\n");
		pam_end(data->pamh, PAM_ABORT);
		fail_exit(data, PAM_ABORT);
	}
	if (error != NULL) {
		debug_msg("userhelper: libuser startup error: %s\n",
			  lu_strerror(error));
		pam_end(data->pamh, PAM_ABORT);
		fail_exit(data, PAM_ABORT);
	}

	/* Look up the user's record. */
	ent = lu_ent_new();
	ret = lu_user_lookup_name(context, user, ent, &error);
	if (ret != TRUE) {
		debug_msg("userhelper: libuser doesn't know the user \"%s\"\n",
			  user);
		pam_end(data->pamh, PAM_ABORT);
		fail_exit(data, PAM_ABORT);
	}
	if (error != NULL) {
		debug_msg("userhelper: libuser doesn't know the user: %s\n",
			  lu_strerror(error));
		pam_end(data->pamh, PAM_ABORT);
		fail_exit(data, PAM_ABORT);
	}

	/* Pull up the user's GECOS data, and split it up. */
	memset(&parsed_gecos, 0, sizeof(parsed_gecos));
	old_gecos = lu_ent_get_first_value_strdup(ent, LU_GECOS);
	if (old_gecos != NULL) {
		gecos_parse(old_gecos, &parsed_gecos);
		debug_msg("userhelper: old gecos string \"%s\"\n", old_gecos);
		debug_msg("userhelper: old gecos \"'%s','%s','%s','%s','%s'\""
			  "\n", parsed_gecos.full_name, parsed_gecos.office,
			  parsed_gecos.office_phone, parsed_gecos.home_phone,
			  parsed_gecos.site_info);
		g_free(old_gecos);
	}

	/* Override any new values we have. */
	if (new_full_name != NULL) {
		g_free(parsed_gecos.full_name);
		parsed_gecos.full_name = g_strdup(new_full_name);
	}
	if (new_office != NULL) {
		g_free(parsed_gecos.office);
		parsed_gecos.office = g_strdup(new_office);
	}
	if (new_office_phone != NULL) {
		g_free(parsed_gecos.office_phone);
		parsed_gecos.office_phone = g_strdup(new_office_phone);
	}
	if (new_home_phone != NULL) {
		g_free(parsed_gecos.home_phone);
		parsed_gecos.home_phone = g_strdup(new_home_phone);
	}
	debug_msg("userhelper: new gecos \"'%s','%s','%s','%s','%s'\"\n",
		  parsed_gecos.full_name ? parsed_gecos.full_name : "(null)",
		  parsed_gecos.office ? parsed_gecos.office : "(null)",
		  parsed_gecos.office_phone ? parsed_gecos.office_phone
		  : "(null)",
		  parsed_gecos.home_phone ? parsed_gecos.home_phone : "(null)",
		  parsed_gecos.site_info ? parsed_gecos.site_info : "(null)");

	/* Verify that the strings we got passed are not too long. */
	if (gecos_size(&parsed_gecos) > GECOS_LENGTH) {
		debug_msg("userhelper: user gecos too long %zu > %d\n",
			  gecos_size(&parsed_gecos), GECOS_LENGTH);
		lu_ent_free(ent);
		lu_end(context);
		pam_end(data->pamh, PAM_ABORT);
		die(data, ERR_FIELDS_INVALID);
	}

	/* Build a new value for the GECOS data. */
	new_gecos = gecos_assemble(&parsed_gecos);
	debug_msg("userhelper: new gecos string \"%s\"\n", new_gecos);

	/* We don't need the user's current GECOS anymore, so set our own in
	   the in-memory structure. */
	lu_ent_set_string(ent, LU_GECOS, new_gecos);
	g_free(new_gecos);

	/* While we're at it, set the individual data items as well. Note that
	 * this may modify old data if LU_GECOS and the individual components
	 * weren???t in sync. */
	if (parsed_gecos.full_name != NULL)
		lu_ent_set_string(ent, LU_COMMONNAME, parsed_gecos.full_name);
	if (parsed_gecos.office != NULL)
		lu_ent_set_string(ent, LU_ROOMNUMBER, parsed_gecos.office);
	if (parsed_gecos.office_phone != NULL)
		lu_ent_set_string(ent, LU_TELEPHONENUMBER,
				  parsed_gecos.office_phone);
	if (parsed_gecos.home_phone != NULL)
		lu_ent_set_string(ent, LU_HOMEPHONE, parsed_gecos.home_phone);
	gecos_free(&parsed_gecos);

	/* If we're here to change the user's shell, too, do that while we're
	 * in here, assuming that chsh and chfn have identical PAM
	 * configurations. */
	if (new_shell != NULL) {
		/* Check that the user's current shell is valid, and that she
		 * is not attempting to change to an invalid shell. */
		old_shell = lu_ent_get_first_value_strdup(ent, LU_LOGINSHELL);
		if (old_shell == NULL)
			old_shell = g_strdup("/bin/sh");

		debug_msg("userhelper: current shell \"%s\"\n", old_shell);
		debug_msg("userhelper: new shell \"%s\"\n", new_shell);
		/* If the old or new shell are invalid, then
		 * the user doesn't get to make the change. */
		if (!shell_valid(new_shell) || !shell_valid(old_shell)) {
			debug_msg("userhelper: bad shell value\n");
			lu_ent_free(ent);
			lu_end(context);
			pam_end(data->pamh, PAM_ABORT);
			fail_exit(data, ERR_SHELL_INVALID);
		}

		/* Set the shell to the new value. */
		lu_ent_set_string(ent, LU_LOGINSHELL, new_shell);
	}

	/* Save the changes to the user's account to the password
	 * database, whereever that is. */
	ret = lu_user_modify(context, ent, &error);
	if (ret != TRUE) {
		lu_ent_free(ent);
		lu_end(context);
		pam_end(data->pamh, PAM_ABORT);
		debug_msg("userhelper: libuser save failed\n");
		fail_exit(data, PAM_ABORT);
	}
	if (error != NULL) {
		debug_msg("userhelper: libuser save error: %s\n",
			  lu_strerror(error));
		lu_ent_free(ent);
		lu_end(context);
		pam_end(data->pamh, PAM_ABORT);
		fail_exit(data, PAM_ABORT);
	}

	lu_ent_free(ent);
	lu_end(context);
	_exit(0);
}

static char *
construct_cmdline(const char *argv0, char **argv)
{
	char *ret;

	if (argv == NULL || argv[0] == NULL)
		return NULL;
	if (argv[1] == NULL)
		ret = g_strdup(argv0);
	else {
		char *tmp;

		tmp = g_strjoinv(" ", argv + 1);
		ret = g_strconcat(argv0, " ", tmp, NULL);
		g_free(tmp);
	}
	return ret;
}

static void G_GNUC_NORETURN
wrap(const char *user, const char *program,
     struct app_data *data, struct pam_conv *text_conv, lu_prompt_fn *prompt,
     int argc, char **argv)
{
	/* We're here to wrap the named program.  After authenticating as the
	 * user given in the console.apps configuration file, execute the
	 * command given in the console.apps file. */
	char *constructed_path;
#ifdef HAVE_FEXECVE
	int fd;
#endif
	char *apps_filename;
	char *user_pam;
	const char *auth_user;
	char *val;
	char **environ_save, **keep_env_names, **keep_env_values;
	char *env_home, *env_term, *env_desktop_startup_id;
	char *env_display, *env_shell;
	char *env_lang, *env_language, *env_lcall, *env_lcmsgs;
	char *env_xauthority;
	int session, tryagain, gui, retval;
	struct stat sbuf;
	struct passwd *pwd;
	shvarFile *s;

	(void)prompt;
	/* Find the basename of the command we're wrapping. */
	if (strrchr(program, '/')) {
		program = strrchr(program, '/') + 1;
	}

	/* Open the console.apps configuration file for this wrapped program,
	 * and read settings from it. */
	apps_filename = g_strconcat(SYSCONFDIR "/security/console.apps/",
				    program, NULL);
	s = svNewFile(apps_filename);

	/* If the file is world-writable, or isn't a regular file, or couldn't
	 * be opened, just exit.  We don't want to alert an attacker that the
	 * service name is invalid. */
	if ((s == NULL) ||
	    (fstat(s->fd, &sbuf) == -1) ||
	    !S_ISREG(sbuf.st_mode) ||
	    (sbuf.st_mode & S_IWOTH)) {
		debug_msg("userhelper: bad file permissions: %s \n",
			  apps_filename);
		die(data, ERR_UNK_ERROR);
	}
	g_free(apps_filename);

	/* Save some of the current environment variables, because the
	 * environment is going to be nuked shortly. */
	env_desktop_startup_id = g_strdup(getenv("DESKTOP_STARTUP_ID"));
	env_display = g_strdup(getenv("DISPLAY"));
	env_home = g_strdup(getenv("HOME"));
	env_lang = g_strdup(getenv("LANG"));
	env_language = g_strdup(getenv("LANGUAGE"));
	env_lcall = g_strdup(getenv("LC_ALL"));
	env_lcmsgs = g_strdup(getenv("LC_MESSAGES"));
	env_shell = g_strdup(getenv("SHELL"));
	env_term = g_strdup(getenv("TERM"));
	env_xauthority = g_strdup(getenv("XAUTHORITY"));

	/* Sanity-check the environment variables as best we can: those
	 * which aren't path names shouldn't contain "/", and none of
	 * them should contain ".." or "%". */
	if (env_display &&
	    (strstr(env_display, "..") ||
	     strchr(env_display, '%'))) {
		g_free(env_display);
		env_display = NULL;
	}
	if (env_home &&
	    (strstr(env_home, "..") ||
	     strchr(env_home, '%'))) {
		g_free(env_home);
		env_home = NULL;
	}
	if (env_lang &&
	    (strstr(env_lang, "/") ||
	     strstr(env_lang, "..") ||
	     strchr(env_lang, '%'))) {
		g_free(env_lang);
		env_lang = NULL;
	}
	if (env_language &&
	    (strstr(env_language, "/") ||
	     strstr(env_language, "..") ||
	     strchr(env_language, '%'))) {
		g_free(env_language);
		env_language = NULL;
	}
	if (env_lcall &&
	    (strstr(env_lcall, "/") ||
	     strstr(env_lcall, "..") ||
	     strchr(env_lcall, '%'))) {
		g_free(env_lcall);
		env_lcall = NULL;
	}
	if (env_lcmsgs &&
	    (strstr(env_lcmsgs, "/") ||
	     strstr(env_lcmsgs, "..") ||
	     strchr(env_lcmsgs, '%'))) {
		g_free(env_lcmsgs);
		env_lcmsgs = NULL;
	}
	if (env_shell &&
	    (strstr(env_shell, "..") ||
	     strchr(env_shell, '%'))) {
		g_free(env_shell);
		env_shell = NULL;
	}
	if (env_term &&
	    (strstr(env_term, "..") ||
	     strchr(env_term, '%'))) {
		g_free(env_term);
		env_term = g_strdup("dumb");
	}
	if (env_xauthority &&
	    (strstr(env_xauthority , "..") ||
	     strchr(env_xauthority , '%'))) {
		g_free(env_xauthority);
		env_xauthority = NULL;
	}

	keep_env_names = NULL;
	keep_env_values = NULL;
	val = svGetValue(s, "KEEP_ENV_VARS");
	if (val != NULL) {
		size_t i, num_names;

		keep_env_names = g_strsplit(val, ",", -1);
		g_free(val);
		if (keep_env_names) {
			num_names = g_strv_length(keep_env_names);
			keep_env_values = g_malloc0_n(num_names,
					      sizeof (*keep_env_values));
			if (keep_env_values)
				for (i = 0; i < num_names; i++)
				/* g_strdup(NULL) is defined to be NULL. */
				keep_env_values[i]
					= g_strdup(getenv(keep_env_names[i]));
		}
		if (keep_env_names == NULL || keep_env_values == NULL) {
			if (keep_env_names) g_strfreev(keep_env_names);
			if (keep_env_values) g_strfreev(keep_env_values);
			keep_env_names = NULL;
			keep_env_values = NULL;
		}
	}

	/* Wipe out the current environment. */
	environ_save = environ;
	environ = g_malloc0(2 * sizeof(char *));

	/* Set just the environment variables we can trust.  Note that
	 * XAUTHORITY is not initially copied -- we don't want to let attackers
	 * get at others' X authority records -- we restore XAUTHORITY below
	 * *after* successfully authenticating, or abandoning authentication in
	 * order to run the wrapped program as the invoking user. */
	if (env_display) {
		setenv("DISPLAY", env_display, 1);
		g_free(env_display);
	}

	/* The rest of the environment variables are simpler. */
	if (env_desktop_startup_id) {
		setenv("DESKTOP_STARTUP_ID", env_desktop_startup_id, 1);
		g_free(env_desktop_startup_id);
	}
	if (env_lang) {
		setenv("LANG", env_lang, 1);
		g_free(env_lang);
	}
	if (env_language) {
		setenv("LANGUAGE", env_language, 1);
		g_free(env_language);
	}
	if (env_lcall) {
		setenv("LC_ALL", env_lcall, 1);
		g_free(env_lcall);
	}
	if (env_lcmsgs) {
		setenv("LC_MESSAGES", env_lcmsgs, 1);
		g_free(env_lcmsgs);
	}
	if (env_shell) {
		setenv("SHELL", env_shell, 1);
		g_free(env_shell);
	}
	if (env_term) {
		setenv("TERM", env_term, 1);
		g_free(env_term);
	}

	/* Set the PATH to a reasonaly safe list of directories. */
	setenv("PATH", "/usr/sbin:/usr/bin:/sbin:/bin:/root/bin", 1);

	/* Set the LOGNAME and USER variables to the executing name. */
	setenv("LOGNAME", "root", 1);
	setenv("USER", "root", 1);

	/* Handle KEEP_ENV_VARS only after setting most of the variables above.
	   This lets the config file to request keeping the value of an
	   environment variable even if it would be otherwise overridden (e.g.
	   PATH). */
	if (keep_env_names != NULL) {
		size_t i;

		for (i = 0; keep_env_names[i] != NULL; i++) {
			if (keep_env_values[i] != NULL) {
				setenv(keep_env_names[i], keep_env_values[i],
				       1);
				g_free(keep_env_values[i]);
			}
		}
		g_strfreev(keep_env_names);
		g_free(keep_env_values);
	}

	/* Pass the original UID to the new program */
	val = g_strdup_printf("%jd", (intmax_t)getuid());
	setenv("USERHELPER_UID", val, 1);
	g_free(val);

	/* We can forcefully disable the GUI from the configuration
	 * file (a la blah-nox applications). */
	gui = svTrueValue(s, "GUI", TRUE);
	/* We can use a magic configuration file option to disable
	 * the GUI, too. */
	if (gui) {
		val = svGetValue(s, "NOXOPTION");
		if (val != NULL && strlen(val) > 1) {
			int i;

			for (i = optind; i < argc; i++) {
				if (strcmp(argv[i], val) == 0) {
					gui = FALSE;
					break;
				}
			}
		}
		g_free(val);
	}

	if (!gui) {
		/* We are not really executing anything yet, but this switches
		   off the parent to a "pass exit code through" mode without
		   displaying any unwanted GUI dialogs. */
		retval = pipe_conv_exec_start(data);
		if (retval != 0) {
			g_strfreev(environ);
			environ = environ_save;
			die(data, retval);
		}
		data->conv = text_conv;
	}

	/* Read the path to the program to run. */
	constructed_path = svGetValue(s, "PROGRAM");
	/* Prefer fexecve to prevent race contitions. */
#ifdef HAVE_FEXECVE
	fd = constructed_path ? open(constructed_path, O_RDONLY) : -1;
	if (!constructed_path || fd < 0 || constructed_path[0] != '/') {
		struct stat statbuf;
		
		if (fd >= 0) close(fd);
#else
	if (!constructed_path || constructed_path[0] != '/') {
#endif
		g_free(constructed_path);
		/* Criminy....  The system administrator didn't give us an
		 * absolute path to the program!  Guess either /usr/sbin or
		 * /sbin, and then give up if we don't find anything by that
		 * name in either of those directories.  FIXME: we're a setuid
		 * app, so access() may not be correct here, as it may give
		 * false negatives.  But then, it wasn't an absolute path. */
		constructed_path = g_strconcat("/usr/sbin/", program, NULL);
#ifdef HAVE_FEXECVE
		if (
			(fd = open(constructed_path, O_RDONLY)) < 0
		    || fstat(fd, &statbuf) < 0
		    || (statbuf.st_mode & UH_S_IXALL) != UH_S_IXALL
		) {
			if (fd >= 0) close(fd);
#else
		if (access(constructed_path, X_OK) != 0) {
#endif
			/* Try the second directory. */
			strcpy(constructed_path, "/sbin/");
			strcat(constructed_path, program);
#ifdef HAVE_FEXECVE
			if (
				(fd = open(constructed_path, O_RDONLY)) < 0
				|| fstat(fd, &statbuf) < 0
				|| (statbuf.st_mode & UH_S_IXALL) != UH_S_IXALL
			) {
				if (fd >= 0) close(fd);
#else
			if (access(constructed_path, X_OK)) {
#endif
				/* Nope, not there, either. */
				debug_msg("userhelper: couldn't find wrapped "
					  "binary\n");
				g_strfreev(environ);
				environ = environ_save;
				die(data, ERR_NO_PROGRAM);
			}
		}
	}
#ifdef HAVE_FEXECVE
    {
		char buf[2];
		ssize_t nread;
		int flags;

        flags = fcntl(fd, F_GETFD);
        if (flags < 0) {
			g_warning("fcntl(%d, F_GETFD) < 0", fd);
			flags = 0;
		}
		nread = read(fd, buf, sizeof(buf));
		lseek(fd, 0, SEEK_SET);
		/* fexecve(2): if executable is a script, don't close on exec */
		if (nread < (ssize_t)sizeof(buf) || buf[0] != '#' || buf[1] != '!')
			if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
				g_warning("fcntl(%d, F_SETFD, FD_CLOEXEC) < 0", fd);
	}
#endif

	user_pam = get_user_for_auth(data, s);
	/* Verify that the user we need to authenticate as has a home
	 * directory. */
	pwd = getpwnam(user_pam);
	if (pwd == NULL) {
		debug_msg("userhelper: no user named %s exists\n", user_pam);
#ifdef HAVE_FEXECVE
		close(fd);
#endif
		g_strfreev(environ);
		environ = environ_save;
		die(data, ERR_NO_USER);
	}

	/* If the user we're authenticating as has root's UID, then it's
	 * safe to let them use HOME=~root. */
	if (pwd->pw_uid == 0)
		setenv("HOME", pwd->pw_dir, 1);
	else {
		/* Otherwise, if they had a reasonable value for HOME, let them
		 * use it. */
		if (env_home != NULL) {
			setenv("HOME", env_home, 1);
			g_free(env_home);
			env_home = NULL;
		}
		else {
			/* Otherwise, set HOME to the user's home directory. */
			pwd = getpwuid(getuid());
			if ((pwd != NULL) && (pwd->pw_dir != NULL))
				setenv("HOME", pwd->pw_dir, 1);
		}
	}
	if (env_home) g_free(env_home);

	/* Read other settings. */
	session = svTrueValue(s, "SESSION", FALSE);
	data->fallback_allowed = svTrueValue(s, "FALLBACK", FALSE);
	val = svGetValue(s, "RETRY"); /* default value is "2" */
	tryagain = val ? atoi(val) + 1 : 3;
	g_free(val);

	/* Read any custom messages we might want to use. */
	val = svGetValue(s, "BANNER");
	if (val != NULL && strlen(val) > 0)
		data->banner = val;
	else if (val != NULL)
		g_free(val);
	val = svGetValue(s, "DOMAIN");
	if (val != NULL && strlen(val) > 0) {
		bindtextdomain(val, LOCALEDIR);
		data->domain = val;
	}
	else if (val != NULL)
		g_free(val);
	if (data->domain == NULL) {
		val = svGetValue(s, "BANNER_DOMAIN");
		if (val != NULL && strlen(val) > 0) {
			bindtextdomain(val, LOCALEDIR);
			data->domain = val;
		}
		else if (val != NULL)
			g_free(val);
	}
	if (data->domain == NULL) {
		data->domain = program;
	}
	val = svGetValue(s, "STARTUP_NOTIFICATION_NAME");
	if (val != NULL && strlen(val) > 0)
		data->sn_name = val;
	else if (val != NULL)
		g_free(val);
	val = svGetValue(s, "STARTUP_NOTIFICATION_DESCRIPTION");
	if (val != NULL && strlen(val) > 0)
		data->sn_description = val;
	else if (val != NULL)
		g_free(val);
	val = svGetValue(s, "STARTUP_NOTIFICATION_WMCLASS");
	if (val != NULL && strlen(val) > 0)
		data->sn_wmclass = val;
	else if (val != NULL)
		g_free(val);
	val = svGetValue(s, "STARTUP_NOTIFICATION_BINARY_NAME");
	if (val != NULL && strlen(val) > 0)
		data->sn_binary_name = val;
	else if (val != NULL)
		g_free(val);
	val = svGetValue(s, "STARTUP_NOTIFICATION_ICON_NAME");
	if (val != NULL && strlen(val) > 0)
		data->sn_icon_name = val;
	else if (val != NULL)
		g_free(val);
	val = svGetValue(s, "STARTUP_NOTIFICATION_WORKSPACE");
	if (val != NULL && strlen(val) > 0)
		data->sn_workspace = atoi(val);

	/* Now we're done reading the file. Close it. */
	svCloseFile(s);

	/* Start up PAM to authenticate the specified user. */
	retval = pam_start(program, user_pam, data->conv, &data->pamh);
	if (retval != PAM_SUCCESS) {
		debug_msg("userhelper: pam_start() failed\n");
#ifdef HAVE_FEXECVE
		close(fd);
#endif
		g_strfreev(environ);
		environ = environ_save;
		fail_exit(data, retval);
	}

	set_pam_items(data, user);

	/* Try to authenticate the user. */
	do {
		debug_msg("userhelper: authenticating \"%s\"\n", user_pam);
		retval = pam_authenticate(data->pamh, 0);
		debug_msg("userhelper: PAM retval = %d (%s)\n", retval,
			  pam_strerror(data->pamh, retval));
		tryagain--;
	} while ((retval != PAM_SUCCESS) && tryagain &&
		 !data->fallback_chosen && !data->canceled);

	if (retval != PAM_SUCCESS) {
		pam_end(data->pamh, retval);
		if (data->canceled) {
#ifdef HAVE_FEXECVE
			close(fd);
#endif
			g_strfreev(environ);
			environ = environ_save;
			fail_exit(data, retval);
		} else
		if (data->fallback_allowed) {
			/* Reset the user's environment so that the
			 * application can run normally. */
			argv[optind - 1] = strdup(program);
			g_strfreev(environ);
			environ = environ_save;
			become_normal(data, user);
			if (data->input != NULL) {
				fflush(data->input);
				if (fcntl(UH_INFILENO, F_SETFD, FD_CLOEXEC) < 0)
					g_warning("fcntl(%d, F_SETFD, FD_CLOEXEC) < 0", UH_INFILENO);
			}
			if (data->output != NULL) {
				fflush(data->output);
				if (fcntl(UH_OUTFILENO, F_SETFD, FD_CLOEXEC) < 0)
					g_warning("fcntl(%d, F_SETFD, FD_CLOEXEC) < 0", UH_OUTFILENO);
			}
			retval = pipe_conv_exec_start(data);
			if (retval != 0) {
#ifdef HAVE_FEXECVE
				close(fd);
#endif
				die(data, retval);
			}
#ifdef USE_STARTUP_NOTIFICATION
			if (data->sn_id) {
				debug_msg("userhelper: setting "
					  "DESKTOP_STARTUP_ID =\"%s\"\n",
					  data->sn_id);
				setenv("DESKTOP_STARTUP_ID",
				       data->sn_id, 1);
			}
#endif
#ifdef USE_FEXECVE
			fexecve(fd, argv + optind - 1, environ);
			close(fd);
#else
			execv(constructed_path, argv + optind - 1);
#endif
			if (data->output != NULL)
				pipe_conv_exec_fail(data);
			die(data, ERR_EXEC_FAILED);
		} else {
			/* Well, we tried. */
#ifdef HAVE_FEXECVE
			close(fd);
#endif
			g_strfreev(environ);
			environ = environ_save;
			fail_exit(data, retval);
		}
	}

	/* Verify that the authenticated user is the user we started
	 * out trying to authenticate. */
	retval = get_pam_string_item(data->pamh, PAM_USER, &auth_user);
	if (retval != PAM_SUCCESS) {
#ifdef HAVE_FEXECVE
		close(fd);
#endif
		pam_end(data->pamh, retval);
		g_strfreev(environ);
		environ = environ_save;
		fail_exit(data, retval);
	}
	if (strcmp(user_pam, auth_user) != 0) {
#ifdef HAVE_FEXECVE
		close(fd);
#endif
		g_strfreev(environ);
		environ = environ_save;
		die(data, ERR_UNK_ERROR);
	}

	/* Verify that the authenticated user is allowed to run this
	 * service now. */
	retval = pam_acct_mgmt(data->pamh, 0);
	if (retval != PAM_SUCCESS) {
#ifdef HAVE_FEXECVE
		close(fd);
#endif
		pam_end(data->pamh, retval);
		g_strfreev(environ);
		environ = environ_save;
		fail_exit(data, retval);
	}

	/* We need to re-read the user's information -- libpam doesn't
	 * guarantee that these won't be nuked. */
	pwd = getpwnam(user_pam);
	if (pwd == NULL) {
		debug_msg("userhelper: no user named %s exists\n", user_pam);
#ifdef HAVE_FEXECVE
		close(fd);
#endif
		g_strfreev(environ);
		environ = environ_save;
		die(data, ERR_NO_USER);
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
			g_free(env_xauthority);
		}

		/* Open a session. */
		retval = pam_open_session(data->pamh, 0);
		if (retval != PAM_SUCCESS) {
#ifdef HAVE_FEXECVE
			close(fd);
#endif
			pam_end(data->pamh, retval);
			g_strfreev(environ);
			environ = environ_save;
			fail_exit(data, retval);
		}

		become_super_supplementary_groups(data);

		retval = pam_setcred(data->pamh, PAM_ESTABLISH_CRED);
		if (retval != PAM_SUCCESS) {
#ifdef HAVE_FEXECVE
			close(fd);
#endif
			pam_end(data->pamh, retval);
			g_strfreev(environ);
			environ = environ_save;
			fail_exit(data, retval);
		}

		/* Start up a child process we can wait on. */
		child = fork();
		if (child == -1) {
#ifdef HAVE_FEXECVE
			close(fd);
#endif
			g_strfreev(environ);
			environ = environ_save;
			die(data, ERR_EXEC_FAILED);
		}
		if (child == 0) {
			/* We're in the child.  Make a few last-minute
			 * preparations and exec the program. */
			char **env_pam;
			const char *cmdline;

			env_pam = pam_getenvlist(data->pamh);
			while (env_pam && *env_pam) {
				debug_msg("userhelper: setting %s\n", *env_pam);
				putenv(g_strdup(*env_pam));
				env_pam++;
			}

			argv[optind - 1] = strdup(program);
			debug_msg("userhelper: about to exec \"%s\"\n",
				  constructed_path);
			/* become_super_supplementary_groups() called in the
			   parent. */
			become_super_other(data);
			if (data->input != NULL) {
				fflush(data->input);
				if (fcntl(UH_INFILENO, F_SETFD, FD_CLOEXEC) < 0)
					g_warning("fcntl(%d, F_SETFD, FD_CLOEXEC) < 0", UH_INFILENO);
			}
			if (data->output != NULL) {
				fflush(data->output);
				if (fcntl(UH_OUTFILENO, F_SETFD, FD_CLOEXEC) < 0)
					g_warning("fcntl(%d, F_SETFD, FD_CLOEXEC) < 0", UH_OUTFILENO);
			}
			retval = pipe_conv_exec_start(data);
			if (retval != 0) {
#ifdef HAVE_FEXECVE
				close(fd);
#endif
				g_strfreev(environ);
				environ = environ_save;
				die(data, retval);
			}
#ifdef USE_STARTUP_NOTIFICATION
			if (data->sn_id) {
				debug_msg("userhelper: setting "
					  "DESKTOP_STARTUP_ID =\"%s\"\n",
					  data->sn_id);
				setenv("DESKTOP_STARTUP_ID",
				       data->sn_id, 1);
			}
#endif
			cmdline = construct_cmdline(constructed_path,
						    argv + optind - 1);
			debug_msg("userhelper: running '%s' with root "
				  "privileges on behalf of '%s'.\n", cmdline,
				  user);
			syslog(LOG_NOTICE, "running '%s' with "
			       "root privileges on behalf of '%s'",
			       cmdline, user);
#ifdef HAVE_FEXECVE
			fexecve(fd, argv + optind - 1, environ);
			close(fd);
#else
			execv(constructed_path, argv + optind - 1);
#endif
			syslog(LOG_ERR, "could not run '%s' with "
			       "root privileges on behalf of '%s': %s",
			       cmdline, user, strerror(errno));
			if (data->output != NULL)
				pipe_conv_exec_fail(data);
			g_strfreev(environ);
			environ = environ_save;
			die(data, ERR_EXEC_FAILED);
		}
		/* We're in the parent.  Wait for the child to exit.  The child
		   is calling pipe_conv_exec_{start,fail} () to define the
		   semantics of its exit code. */
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);

		waitpid(child, &status, 0);

		/* Close the session. */
		retval = pam_close_session(data->pamh, 0);
		if (retval != PAM_SUCCESS) {
			pipe_conv_exec_fail(data);
			pam_setcred(data->pamh, PAM_DELETE_CRED);
			pam_end(data->pamh, retval);
#ifdef HAVE_FEXECVE
			close(fd);
#endif
			g_strfreev(environ);
			environ = environ_save;
			fail_exit(data, retval);
		}

		pam_setcred(data->pamh, PAM_DELETE_CRED);
		pam_end(data->pamh, PAM_SUCCESS);
#ifdef HAVE_FEXECVE
		close(fd);
#endif
		g_strfreev(environ);
		environ = environ_save;
		if (WIFEXITED(status))
			exit(WEXITSTATUS(status));
		if (WIFSIGNALED(status))
			exit(WTERMSIG(status) + 128);
		exit(255);
	} else {
		const char *cmdline;

		if (env_xauthority) g_free(env_xauthority);
		/* We're not opening a session, so we can just exec()
		 * the program we're wrapping. */
		pam_end(data->pamh, PAM_SUCCESS);

		argv[optind - 1] = strdup(program);
		debug_msg("userhelper: about to exec \"%s\"\n",
			  constructed_path);
		become_super(data);
		if (data->input != NULL) {
			fflush(data->input);
			if (fcntl(UH_INFILENO, F_SETFD, FD_CLOEXEC) < 0)
				g_warning("fcntl(%d, F_SETFD, FD_CLOEXEC) < 0", UH_INFILENO);
		}
		if (data->output != NULL) {
			fflush(data->output);
			if (fcntl(UH_OUTFILENO, F_SETFD, FD_CLOEXEC) < 0)
				g_warning("fcntl(%d, F_SETFD, FD_CLOEXEC) < 0", UH_OUTFILENO);
		}
		retval = pipe_conv_exec_start(data);
		if (retval != 0) {
#ifdef HAVE_FEXECVE
			close(fd);
#endif
			g_strfreev(environ);
			environ = environ_save;
			die(data, retval);
		}
#ifdef USE_STARTUP_NOTIFICATION
		if (data->sn_id) {
			debug_msg("userhelper: setting "
				  "DESKTOP_STARTUP_ID =\"%s\"\n", data->sn_id);
			setenv("DESKTOP_STARTUP_ID", data->sn_id, 1);
		}
#endif
		cmdline = construct_cmdline(constructed_path,
					    argv + optind - 1);
		debug_msg("userhelper: running '%s' with root privileges "
			  "on behalf of '%s'\n", cmdline, user);
		syslog(LOG_NOTICE, "running '%s' with "
		       "root privileges on behalf of '%s'",
		       cmdline, user);

#ifdef HAVE_FEXECVE
		fexecve(fd, argv + optind -1, environ);
		close(fd);
#else
		execv(constructed_path, argv + optind - 1);
#endif
		syslog(LOG_ERR, "could not run '%s' with "
		       "root privileges on behalf of '%s': %s",
		       cmdline, user, strerror(errno));
		pipe_conv_exec_fail(data);
		g_strfreev(environ);
		environ = environ_save;
		die(data, ERR_EXEC_FAILED);
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
	struct passwd *pwd;
	lu_prompt_fn *prompt;
	char *user_name; /* current user, as determined by real uid */
	int f_flag;	 /* -f flag = change full name */
	int o_flag;	 /* -o flag = change office name */
	int p_flag;	 /* -p flag = change office phone */
	int h_flag;	 /* -h flag = change home phone number */
	int c_flag;	 /* -c flag = change password */
	int s_flag;	 /* -s flag = change shell */
	int t_flag;	 /* -t flag = direct interactive text-mode -- exec'ed */
	int w_flag;	 /* -w flag = act as a wrapper for next * args */
	const char *new_full_name;
	const char *new_office;
	const char *new_office_phone;
	const char *new_home_phone;
	const char *new_shell;
#ifdef WITH_SELINUX
	security_class_t class;
	access_vector_t perm;
#endif

	/* State variable we pass around. */
	struct app_data app_data = {
		NULL,
		NULL,
		FALSE, FALSE, FALSE,
		NULL, NULL,
		NULL, NULL,
		NULL, NULL, NULL,
		NULL, NULL, NULL,
		-1,
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

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	openlog("userhelper", LOG_PID, LOG_AUTHPRIV);

	if (geteuid() != 0) {
		fprintf(stderr, _("userhelper must be setuid root\n"));
		debug_msg("userhelper: not setuid\n");
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
				debug_msg("userhelper: invalid call: "
					  "unknown option\n");
				exit(ERR_INVALID_CALL);
		}
	}

	/* Sanity-check the arguments a bit. */
#define SHELL_FLAGS (f_flag || o_flag || h_flag || p_flag || s_flag)
	if ((c_flag && SHELL_FLAGS) ||
	    (c_flag && w_flag) ||
	    (w_flag && SHELL_FLAGS)) {
		debug_msg("userhelper: invalid call: invalid combination of "
			  "options\n");
		exit(ERR_INVALID_CALL);
	}

	/* Determine which conversation function to use. */
	if (t_flag) {
		/* We were told to use text mode. */
		if (isatty(STDIN_FILENO)) {
			/* We have a controlling tty on which we can disable
			 * echoing, so use the text conversation method. */
			app_data.conv = &text_conv;
		} else {
			/* We have no controlling terminal -- being run from
			 * cron or some other mechanism? */
			app_data.conv = &silent_conv;
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
			debug_msg("userhelper: invalid call\n");
			exit(ERR_INVALID_CALL);
		}
		app_data.conv = &pipe_conv;
		prompt = &prompt_pipe;
	}

	user_name = get_invoking_user(&app_data);
	debug_msg("userhelper: current user is %s\n", user_name);

	/* If we didn't get the -w flag, the last argument can be a user's
	 * name. */
	if (w_flag == 0) {
		if ((getuid() == 0) && (argc == optind + 1)) {
			/* We were started by the superuser, so accept the
			 * user's name. */
			g_free(user_name);
			user_name = g_strdup(argv[optind]);

#ifdef WITH_SELINUX
			class = string_to_security_class("passwd");
			if (c_flag) 
			  perm = string_to_av_perm(class, "passwd");
			else if (s_flag)
			  perm = string_to_av_perm(class, "chsh");
			else
			  perm = string_to_av_perm(class, "chfn");

			if (is_selinux_enabled() > 0 &&
			    checkAccess(perm)!= 0) {
				security_context_t context = NULL;
				getprevcon(&context);
				syslog(LOG_NOTICE, 
				       "SELinux context %s is not allowed to change information for user \"%s\"\n",
				       context, user_name);
				g_free(user_name);
				die(&app_data, ERR_NO_USER);
			}
#endif
			debug_msg("userhelper: modifying account data for %s\n",
				  user_name);
		} else if (optind != argc) {
			fprintf(stderr,
				_("Unexpected command-line arguments\n"));
			exit(ERR_INVALID_CALL);
		}

		/* Verify that the user exists. */
		pwd = getpwnam(user_name);
		if ((pwd == NULL) || (pwd->pw_name == NULL)) {
			debug_msg("userhelper: user %s doesn't exist\n",
				  user_name);
			die(&app_data, ERR_NO_USER);
		}
	}

	/* Change password? */
	if (c_flag) {
		passwd(user_name, &app_data);
		g_assert_not_reached();
	}

	/* Change GECOS data or shell? */
	if (SHELL_FLAGS) {
		chfn(user_name, &app_data, prompt,
		     new_full_name, new_office,
		     new_office_phone, new_home_phone,
		     new_shell);
		g_assert_not_reached();
	}

	/* Wrap some other program? */
	if (w_flag) {
		wrap(user_name, wrapped_program, &app_data, &text_conv, prompt,
		     argc, argv);
		g_assert_not_reached();
	}

	/* Not reached. */
	g_assert_not_reached();
	exit(0);
}
