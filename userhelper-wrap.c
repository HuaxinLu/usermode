/*
 * Copyright (C) 1997, 2001-2003, 2007 Red Hat, Inc.
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
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#ifdef USE_STARTUP_NOTIFICATION
#include <libsn/sn.h>
#include <libwnck/libwnck.h>
#endif
#include "userdialogs.h"
#include "userhelper.h"
#include "userhelper-wrap.h"

#define  PAD 8
#define  RESPONSE_FALLBACK 100
static int childin[2];
static int childpid;
static int childout_tag;
static int child_exit_status;
static gboolean child_success_dialog = TRUE;
static gboolean child_was_execed = FALSE;

typedef struct message {
	int type;
	GtkWidget *entry;
	GtkWidget *label;
} message;

struct response {
	int responses, rows;
	gboolean ready, fallback_allowed;
	char *user, *service, *suggestion, *banner, *title;
	GList *message_list; /* contains pointers to messages */
	void *dialog; /* Actually a GtkWidget * */
	GtkWidget *first, *last, *table;
};

#ifdef USE_STARTUP_NOTIFICATION
static char *sn_id = NULL;
static char *sn_name = NULL;
static char *sn_description = NULL;
static int sn_workspace = -1;
static char *sn_wmclass = NULL;
static char *sn_binary_name = NULL;
static char *sn_icon_name = NULL;

/* Push errors for the specified display. */
static void
trap_push(SnDisplay *display, Display *xdisplay)
{
	sn_display_error_trap_push(display);
	gdk_error_trap_push();
}

/* Pop errors for the specified display. */
static void
trap_pop(SnDisplay *display, Display *xdisplay)
{
	gdk_error_trap_pop();
	sn_display_error_trap_pop(display);
}

/* Complete startup notification for consolehelper. */
static void
userhelper_startup_notification_launchee(const char *id)
{
	GdkDisplay *gdisp;
	GdkScreen *gscreen;
	SnDisplay *disp;
	SnLauncheeContext *ctx;
	int screen;

	gdisp = gdk_display_get_default();
	gscreen = gdk_display_get_default_screen(gdisp);
	disp = sn_display_new(GDK_DISPLAY(), trap_push, trap_pop);
	screen = gdk_screen_get_number(gscreen);
	if (id == NULL) {
		ctx = sn_launchee_context_new_from_environment(disp, screen);
	} else {
		ctx = sn_launchee_context_new(disp, screen, id);
	}
	if (ctx != NULL) {
#ifdef DEBUG_USERHELPER
		fprintf(stderr, "Completing startup notification for \"%s\".\n",
			sn_launchee_context_get_startup_id(ctx) ?
			sn_launchee_context_get_startup_id(ctx) : "?");
#endif
		sn_launchee_context_complete(ctx);
		sn_launchee_context_unref(ctx);
	}
	sn_display_unref(disp);
}

/* Setup startup notification for our child. */
static void
userhelper_startup_notification_launcher(void)
{
	GdkDisplay *gdisp;
	GdkScreen *gscreen;
	SnDisplay *disp;
	SnLauncherContext *ctx;
	int screen;

	if (sn_name == NULL) {
		return;
	}

	gdisp = gdk_display_get_default();
	gscreen = gdk_display_get_default_screen(gdisp);
	disp = sn_display_new(GDK_DISPLAY(), trap_push, trap_pop);
	screen = gdk_screen_get_number(gscreen);
	ctx = sn_launcher_context_new(disp, screen);

	if (sn_name) {
		sn_launcher_context_set_name(ctx, sn_name);
	}
	if (sn_description) {
		sn_launcher_context_set_description(ctx, sn_description);
	}
	if (sn_workspace != -1) {
		sn_launcher_context_set_workspace(ctx, sn_workspace);
	}
	if (sn_wmclass) {
		sn_launcher_context_set_wmclass(ctx, sn_wmclass);
	}
	if (sn_binary_name) {
		sn_launcher_context_set_binary_name(ctx, sn_binary_name);
	}
	if (sn_icon_name) {
		sn_launcher_context_set_binary_name(ctx, sn_icon_name);
	}
#ifdef DEBUG_USERHELPER
	fprintf(stderr, "Starting launch of \"%s\", id=\"%s\".\n",
		sn_description ? sn_description : sn_name, sn_id);
#endif
	sn_launcher_context_initiate(ctx, "userhelper", sn_name, CurrentTime);
	if (sn_launcher_context_get_startup_id(ctx) != NULL) {
		sn_id = g_strdup(sn_launcher_context_get_startup_id(ctx));
	}

	sn_launcher_context_unref(ctx);
	sn_display_unref(disp);
}
#endif

/* Call gtk_main_quit. */
void
userhelper_main_quit(void)
{
#ifdef DEBUG_USERHELPER
	fprintf(stderr, "Quitting main loop %d.\n", gtk_main_level());
#endif
	gtk_main_quit();
}

static void
userhelper_fatal_error(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	(void)widget;
	(void)event;
	(void)user_data;
	userhelper_main_quit();
}


/* Display a dialog explaining a child's exit status, and exit ourselves. */
static void
userhelper_parse_exitstatus(int exitstatus)
{
	static const struct {
		int code;
		GtkWidget* (*create)(const gchar*, const gchar*);
		const char *message;
	} codes[] = {
		{-1, create_error_box,
		 N_("Unknown exit code.")},
		{0, create_message_box,
		 N_("Information updated.")},
		{ERR_PASSWD_INVALID, create_error_box,
		 N_("The password you typed is invalid.\nPlease try again.")},
		{ERR_FIELDS_INVALID, create_error_box,
		 N_("One or more of the changed fields is invalid.\nThis is probably due to either colons or commas in one of the fields.\nPlease remove those and try again.")},
		{ERR_SET_PASSWORD, create_error_box,
		 N_("Password resetting error.")},
		{ERR_LOCKS, create_error_box,
		 N_("Some systems files are locked.\nPlease try again in a few moments.")},
		{ERR_NO_USER, create_error_box,
		 N_("Unknown user.")},
		{ERR_NO_RIGHTS, create_error_box,
		 N_("Insufficient rights.")},
		{ERR_INVALID_CALL, create_error_box,
		 N_("Invalid call to subprocess.")},
		{ERR_SHELL_INVALID, create_error_box,
		 N_("Your current shell is not listed in /etc/shells.\nYou are not allowed to change your shell.\nConsult your system administrator.")},
		/* well, this is unlikely to work, but at least we tried... */
		{ERR_NO_MEMORY, create_error_box,
		 N_("Out of memory.")},
		{ERR_EXEC_FAILED, create_error_box,
		 N_("The exec() call failed.")},
		{ERR_NO_PROGRAM, create_error_box,
		 N_("Failed to find selected program.")},
		/* special no-display dialog */
		{ERR_CANCELED, NULL,
		 N_("Request canceled.")},
		{ERR_PAM_INT_ERROR, create_error_box,
		 N_("Internal PAM error occured.")},
		{ERR_UNK_ERROR, create_error_box,
		 N_("Unknown error.")},
	};

	GtkWidget *message_box;
	size_t i, code;

#ifdef DEBUG_USERHELPER
	if (child_was_execed) {
		fprintf(stderr, "Wrapped application returned exit status %d.\n", exitstatus);
	} else {
		fprintf(stderr, "Child returned exit status %d.\n", exitstatus);
	}
#endif

	/* If the exit status came from what the child execed, then we don't
	 * care about reporting it to the user. */
	if (child_was_execed) {
		child_exit_status = exitstatus;
		return;
	}

	/* Create a dialog suitable for displaying this code. */
	message_box = NULL;
	code = 0;
	for (i = 1; i < G_N_ELEMENTS(codes); i++) {
		/* If entries past zero match this exit code, we'll use them. */
		if (codes[i].code == exitstatus) {
			code = i;
#ifdef DEBUG_USERHELPER
			fprintf(stderr, "Status is \"%s\".\n",
				codes[i].message);
#endif
			break;
		}
	}

	/* If we recognize this code, create the error dialog for it if we
	   need to display one. */
	if ((code < G_N_ELEMENTS(codes)) && (codes[code].create != NULL)) {
		message_box = codes[code].create(_(codes[code].message), NULL);
	}

	/* Run the dialog box. */
	if (message_box != NULL) {
		if (child_success_dialog || (exitstatus != 0)) {
			gtk_dialog_run(GTK_DIALOG(message_box));
		}
		gtk_widget_destroy(message_box);
	}
}

/* Attempt to grab focus for the toplevel of this widget, so that peers can
 * get events too. */
static void
userhelper_grab_keyboard(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	(void)event;
	(void)user_data;
#ifndef DEBUG_USERHELPER
	GdkGrabStatus ret;

	ret = gdk_keyboard_grab(widget->window, TRUE, GDK_CURRENT_TIME);
	if (ret != GDK_GRAB_SUCCESS) {
		switch (ret) {
		case GDK_GRAB_ALREADY_GRABBED:
			g_warning("keyboard grab failed: keyboard already grabbed by another window");
			break;
		case GDK_GRAB_INVALID_TIME:
			g_warning("keyboard grab failed: requested grab time was invalid (shouldn't happen)");
			break;
		case GDK_GRAB_NOT_VIEWABLE:
			g_warning("keyboard grab failed: window requesting grab is not viewable");
			break;
		case GDK_GRAB_FROZEN:
			g_warning("keyboard grab failed: keyboard is already grabbed");
			break;
		default:
			g_warning("keyboard grab failed: unknown error %d", (int) ret);
			break;
		}
	}
#endif
}

/* Handle the executed dialog, writing responses back to the child when
 * possible. */
static void
userhelper_write_childin(GtkResponseType response, struct response *resp)
{
	const char *input;
	guchar byte;
	GList *message_list = resp->message_list;
	gboolean startup = FALSE;

	switch (response) {
		case RESPONSE_FALLBACK:
			/* The user wants to run unprivileged. */
			byte = UH_FALLBACK;
#ifdef DEBUG_USERHELPER
			fprintf(stderr, "Responding FALLBACK.\n");
#endif
			write(childin[1], &byte, 1);
			write(childin[1], "\n", 1);
			startup = TRUE;
			break;
		case GTK_RESPONSE_CANCEL:
			/* The user doesn't want to run this after all. */
			byte = UH_CANCEL;
#ifdef DEBUG_USERHELPER
			fprintf(stderr, "Responding CANCEL.\n");
#endif
			write(childin[1], &byte, 1);
			write(childin[1], "\n", 1);
			startup = FALSE;
			break;
		case GTK_RESPONSE_OK:
			/* The user answered the questions. */
			byte = UH_TEXT;
			for (message_list = resp->message_list;
			     (message_list != NULL) &&
			     (message_list->data != NULL);
			     message_list = g_list_next(message_list)) {
				message *m = (message *) message_list->data;
#ifdef DEBUG_USERHELPER
				fprintf(stderr, "message %d\n", m->type);
				if (GTK_IS_ENTRY(m->entry)) {
					fprintf(stderr, "Responding `%s'.\n",
						gtk_entry_get_text(GTK_ENTRY(m->entry)));
				}
#endif
				if (GTK_IS_ENTRY(m->entry)) {
					input =
					    gtk_entry_get_text(GTK_ENTRY
							       (m->entry));
					write(childin[1], &byte, 1);
					write(childin[1], input, strlen(input));
					write(childin[1], "\n", 1);
				}
			}
			startup = TRUE;
			break;
		default:
			/* We were closed, deleted, canceled, or something else
			 * which we can treat as a cancellation. */
			startup = FALSE;
			_exit(1);
			break;
	}
#ifdef USE_STARTUP_NOTIFICATION
	if (startup) {
		/* If we haven't set up notification yet, do so. */
		if ((sn_name != NULL) && (sn_id == NULL)) {
			userhelper_startup_notification_launcher();
		}
		/* Tell the child what its ID is. */
		if ((sn_name != NULL) && (sn_id != NULL)) {
#ifdef DEBUG_USERHELPER
			fprintf(stderr, "Sending new window startup ID "
				"\"%s\".\n", sn_id);
#endif
			byte = UH_SN_ID;
			write(childin[1], &byte, 1);
			write(childin[1], sn_id, strlen(sn_id));
			write(childin[1], "\n", 1);
		}
	}
#endif
	/* Tell the child we have no more to say. */
#ifdef DEBUG_USERHELPER
	fprintf(stderr, "Sending synchronization point.\n");
#endif
	byte = UH_SYNC_POINT;
	write(childin[1], &byte, 1);
	write(childin[1], "\n", 1);
}

/* Glue. */
static void
fake_respond_ok(GtkWidget *widget, gpointer user_data)
{
	(void)widget;
	gtk_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_OK);
}

/* Parse requests from the child userhelper process and display them in a
 * message box. */
static void
userhelper_parse_childout(char *outline)
{
	char *prompt;
	int prompt_type;
	static struct response *resp = NULL;

	if (resp == NULL) {
		/* Allocate the response structure. */
		resp = g_malloc0(sizeof(struct response));

		/* Create a table to hold the entry fields and labels. */
		resp->table = gtk_table_new(2, 1, FALSE);
		/* The First row is used for the "Authenticating as \"%s\""
		   label. */
		resp->rows = 1;
	}

	/* Now process items from the child. */
	while ((outline != NULL) && isdigit(outline[0])) {
		gboolean echo;

		/* Allocate a structure to hold the message data. */
		message *msg = g_malloc(sizeof(message));

		/* Read the prompt type. */
		prompt_type = strtol(outline, &prompt, 10);

		/* The first character which wasn't a digit might be whitespace,
		 * so skip over any whitespace before settling on the actual
		 * prompt. */
		if ((prompt != NULL) && (strlen(prompt) > 0)) {
			while ((isspace((unsigned char)prompt[0]) &&
			       (prompt[0] != '\0') &&
			       (prompt[0] != '\n'))) {
				prompt++;
			}
		}

		/* Snip off terminating newlines in the prompt string and save
		 * a pointer to interate the parser along. */
		outline = strchr(prompt, '\n');
		while (outline != NULL) {
			if ((outline[1] == '\0') || (isdigit(outline[1]))) {
				break;
			} else {
				outline = strchr(outline + 1, '\n');
			}
		}
		if (outline != NULL) {
			outline[0] = '\0';
			outline++;
			if (outline[0] == '\0') {
				outline = NULL;
			}
		}

#ifdef DEBUG_USERHELPER
		g_print("Child message: (%d)/\"%s\"\n", prompt_type, prompt);
#endif
		msg->type = prompt_type;
		msg->entry = NULL;

		echo = TRUE;
		switch (prompt_type) {
			/* A suggestion for the next input. */
			case UH_PROMPT_SUGGESTION:
				g_free(resp->suggestion);
				resp->suggestion = g_strdup(prompt);
#ifdef DEBUG_USERHELPER
				g_print("Suggested response \"%s\".\n",
					resp->suggestion);
#endif
				break;
			/* Prompts.  Create a label and entry field. */
			case UH_ECHO_OFF_PROMPT:
				echo = FALSE;
				/* fall through */
			case UH_ECHO_ON_PROMPT:
				/* Only set the title to "Query" if it isn't
				 * already set to "Error" or something else
				 * more meaningful. */
				if (resp->title == NULL) {
					resp->title = _("Query");
				}
				/* Create a label to hold the prompt, and make
				 * a feeble gesture at being accessible :(. */
				msg->label =
					gtk_label_new_with_mnemonic(_(prompt));
				gtk_label_set_line_wrap(GTK_LABEL(msg->label),
							TRUE);
				gtk_misc_set_alignment(GTK_MISC(msg->label),
						       1.0, 0.5);

				/* Create an entry field to hold the answer. */
				msg->entry = gtk_entry_new();
				gtk_label_set_mnemonic_widget(GTK_LABEL(msg->label),
							      GTK_WIDGET(msg->entry));
				gtk_entry_set_visibility(GTK_ENTRY(msg->entry),
							 echo);

				/* If we had a suggestion, use it up. */
				if (resp->suggestion) {
					gtk_entry_set_text(GTK_ENTRY(msg->entry),
							   resp->suggestion);
					g_free(resp->suggestion);
					resp->suggestion = NULL;
				}

				/* Keep track of the first entry field in the
				 * dialog box. */
				if (resp->first == NULL) {
					resp->first = msg->entry;
				}

				/* Keep track of the last entry field in the
				 * dialog box. */
				resp->last = msg->entry;

				/* Insert them. */
				gtk_table_attach(GTK_TABLE(resp->table),
						 msg->label, 0, 1,
						 resp->rows, resp->rows + 1,
						 GTK_EXPAND | GTK_FILL, 0,
						 PAD, PAD);
				gtk_table_attach(GTK_TABLE(resp->table),
						 msg->entry, 1, 2,
						 resp->rows, resp->rows + 1,
						 GTK_EXPAND | GTK_FILL, 0,
						 PAD, PAD);

				/* Add this message to the list of messages. */
				resp->message_list =
					g_list_append(resp->message_list, msg);

				/* Note that this one needs a response. */
				resp->responses++;
				resp->rows++;
#ifdef DEBUG_USERHELPER
				g_print("Now we need %d responses.\n",
					resp->responses);
#endif
				break;
			/* Fallback flag.  Read it and save it for later. */
			case UH_FALLBACK_ALLOW:
				resp->fallback_allowed = atoi(prompt) != 0;
#ifdef DEBUG_USERHELPER
				g_print("Fallback %sallowed.\n",
					resp->fallback_allowed ? "" : "not ");
#endif
				break;
			/* User name. Read it and save it for later. */
			case UH_USER:
				g_free(resp->user);
				resp->user = g_strdup(prompt);
#ifdef DEBUG_USERHELPER
				g_print("User is \"%s\".\n", resp->user);
#endif
				break;
			/* Service name. Read it and save it for later. */
			case UH_SERVICE_NAME:
				g_free(resp->service);
				resp->service = g_strdup(prompt);
#ifdef DEBUG_USERHELPER
				g_print("Service is \"%s\".\n", resp->service);
#endif
				break;
			/* An error message. */
			case UH_ERROR_MSG:
				resp->title = _("Error");
				msg->label = gtk_label_new(_(prompt));
				gtk_table_attach(GTK_TABLE(resp->table),
						 msg->label, 0, 2,
						 resp->rows, resp->rows + 1,
						 0, 0, PAD, PAD);
				resp->message_list =
					g_list_append(resp->message_list, msg);
				resp->rows++;
				break;
			/* An informational message. */
			case UH_INFO_MSG:
				resp->title = _("Information");
				msg->label = gtk_label_new(_(prompt));
				gtk_table_attach(GTK_TABLE(resp->table),
						 msg->label, 0, 2,
						 resp->rows, resp->rows + 1,
						 0, 0, PAD, PAD);
				resp->message_list =
					g_list_append(resp->message_list, msg);
				resp->rows++;
				break;
			/* An informative banner. */
			case UH_BANNER:
				g_free(resp->banner);
				resp->banner = g_strdup(prompt);
#ifdef DEBUG_USERHELPER
				g_print("Banner is \"%s\".\n", resp->banner);
#endif
				break;
			/* Userhelper is trying to exec. */
			case UH_EXEC_START:
				child_was_execed = TRUE;
#ifdef DEBUG_USERHELPER
				g_print("Child started.\n");
#endif
				break;
			/* Userhelper failed to exec. */
			case UH_EXEC_FAILED:
				child_was_execed = FALSE;
#ifdef DEBUG_USERHELPER
				g_print("Child failed.\n");
#endif
				break;
#ifdef USE_STARTUP_NOTIFICATION
			/* Startup notification name. */
			case UH_SN_NAME:
				g_free(sn_name);
				sn_name = g_strdup(prompt);
#ifdef DEBUG_USERHELPER
				g_print("SN Name is \"%s\".\n", sn_name);
#endif
				break;
			/* Startup notification description. */
			case UH_SN_DESCRIPTION:
				g_free(sn_description);
				sn_description = g_strdup(prompt);
#ifdef DEBUG_USERHELPER
				g_print("SN Description is \"%s\".\n",
					sn_description);
#endif
				break;
			/* Startup notification workspace. */
			case UH_SN_WORKSPACE:
				sn_workspace = atoi(prompt);
#ifdef DEBUG_USERHELPER
				g_print("SN Workspace is %d.\n", sn_workspace);
#endif
				break;
			/* Startup notification wmclass. */
			case UH_SN_WMCLASS:
				g_free(sn_wmclass);
				sn_wmclass = g_strdup(prompt);
#ifdef DEBUG_USERHELPER
				g_print("SN WMClass is \"%s\".\n", sn_wmclass);
#endif
				break;
			/* Startup notification binary name. */
			case UH_SN_BINARY_NAME:
				g_free(sn_binary_name);
				sn_binary_name = g_strdup(prompt);
#ifdef DEBUG_USERHELPER
				g_print("SN Binary name is \"%s\".\n",
					sn_binary_name);
#endif
				break;
			/* Startup notification icon name. */
			case UH_SN_ICON_NAME:
				g_free(sn_icon_name);
				sn_icon_name = g_strdup(prompt);
#ifdef DEBUG_USERHELPER
				g_print("SN Icon name is \"%s\".\n",
					sn_icon_name);
#endif
				break;
#endif
			/* Sanity-check for the number of expected responses. */
			case UH_EXPECT_RESP:
				if (resp->responses != atoi(prompt)) {
					fprintf(stderr,
						"Protocol error (%d responses "
						"expected from %d prompts)!\n",
						atoi(prompt), resp->responses);
					_exit(1);
				}
				break;
			/* Synchronization point -- no more prompts. */
			case UH_SYNC_POINT:
				resp->ready = TRUE;
				break;
			default:
				break;
		}
	}

	/* Check that we used up all of the data. */
	if (outline && (strlen(outline) > 0)) {
		fprintf(stderr, "ERROR: unused data: `%s'.\n", outline);
	}

#ifdef USE_STARTUP_NOTIFICATION
	/* Complete startup notification for consolehelper. */
	userhelper_startup_notification_launchee(NULL);
#endif

	if (!resp->ready)
		return;
	/* If we're ready, do some last-minute changes and run the dialog. */
	if (resp->responses == 0)
		/* No queries means that we've just processed a sync request
		   for cases where we don't need any info for authentication.

		   This can happens when the child needs to wait for
		   UH_EXEC_START or UH_EXEC_FAILED response to make sure the
		   exit status is processed correctly.

		   Otherwise, this might be just a module being stupid and
		   calling the conversation callback once. for. every. chunk.
		   of. output and we'll get an actual prompt (which will give
		   us cause to open a dialog) later. */
		userhelper_write_childin(GTK_RESPONSE_OK, resp);
	else {
		/* A non-zero number of queries demands an answer. */
		char *text;
		GtkWidget *label, *image, *vbox;
		GtkResponseType response;

#ifdef DEBUG_USERHELPER
		{
		int timeout = 2;
		g_print("Ready to ask %d questions.\n", resp->responses);
		g_print("Pausing for %d seconds for debugging.\n", timeout);
		sleep(timeout);
		}
#endif

		/* Create a new GTK dialog box. */
		resp->dialog = gtk_message_dialog_new(NULL,
						      0,
						      resp->responses > 0 ?
						      GTK_MESSAGE_QUESTION :
						      GTK_MESSAGE_INFO,
						      resp->responses > 0 ?
						      GTK_BUTTONS_OK_CANCEL :
						      GTK_BUTTONS_CLOSE,
						      _("Placeholder text."));

		/* Ensure that we don't get dangling crap widget pointers. */
		g_object_add_weak_pointer(G_OBJECT(resp->dialog),
					  &resp->dialog);

		/* If we didn't get a title from userhelper, assume badness. */
		gtk_window_set_title(GTK_WINDOW(resp->dialog),
				     resp->title ? resp->title : _("Error"));

		/* Force GTK+ to try to center this dialog. */
		gtk_window_set_position(GTK_WINDOW(resp->dialog),
					GTK_WIN_POS_CENTER_ALWAYS);

		/* Set window to be always on top. */
		gtk_window_set_keep_above(GTK_WINDOW(resp->dialog), TRUE);
		
		/* Set window icon */
		gtk_window_set_icon_from_file(GTK_WINDOW(resp->dialog),
					      DATADIR "/pixmaps/password.png",
					      NULL);

		/* If we're asking questions, change the dialog's icon... */
		if (resp->responses > 0) {
			image = (GTK_MESSAGE_DIALOG(resp->dialog))->image;
			gtk_image_set_from_file(GTK_IMAGE(image),
						DATADIR "/pixmaps/keyring.png");
			/* ... and tell the user which user's passwords to
			 * enter */
			if (resp->user != NULL) {
				text = g_strdup_printf(_("Authenticating as \"%s\""),
						       resp->user);
				label = gtk_label_new(text);
				g_free(text);
				gtk_table_attach(GTK_TABLE(resp->table), label,
						 0, 2, 0, 1, 0, 0, PAD, PAD);
			}
		}

		/* Pack the table into the dialog box. */
		vbox = (GTK_DIALOG(resp->dialog))->vbox;
		gtk_box_pack_start_defaults(GTK_BOX(vbox), resp->table);

		/* Make sure we grab the keyboard focus when the window gets
		 * an X window associated with it. */
		g_signal_connect(G_OBJECT(resp->dialog), "map_event",
				 G_CALLBACK(userhelper_grab_keyboard),
				 NULL);

		/* If the user closes the window, we bail. */
		g_signal_connect(G_OBJECT(resp->dialog), "delete_event",
				 G_CALLBACK(userhelper_fatal_error),
				 NULL);

		/* Customize the label. */
		if (resp->responses == 0)
			text = g_strdup("");
		else if (resp->service != NULL) {
			if (strcmp(resp->service, "passwd") == 0)
				text = g_strdup(_("Changing password."));
			else if (strcmp(resp->service, "chfn") == 0)
				text = g_strdup(_("Changing personal "
						  "information."));
			else if (strcmp(resp->service, "chsh") == 0)
				text = g_strdup(_("Changing login shell."));
			else if (resp->banner != NULL)
				text = g_strdup(resp->banner);
			else if (resp->fallback_allowed)
				text = g_strdup_printf(_("You are attempting to run \"%s\" which may benefit from administrative privileges, but more information is needed in order to do so."), resp->service);
			else
				text = g_strdup_printf(_("You are attempting to run \"%s\" which requires administrative privileges, but more information is needed in order to do so."), resp->service);
		} else {
			if (resp->banner)
				text = g_strdup(resp->banner);
			else if (resp->fallback_allowed)
				text = g_strdup(_("You are attempting to run a command which may benefit from administrative privileges, but more information is needed in order to do so."));
			else
				text = g_strdup(_("You are attempting to run a command which requires administrative privileges, but more information is needed in order to do so."));
		}
		label = (GTK_MESSAGE_DIALOG(resp->dialog))->label;
		gtk_label_set_text(GTK_LABEL(label), text);
		g_free(text);

		/* Add an "unprivileged" button if we're allowed to offer
		 * unprivileged execution as an option. */
		if ((resp->fallback_allowed) && (resp->responses > 0)) {
			gtk_dialog_add_button(GTK_DIALOG(resp->dialog),
					      _("_Run Unprivileged"),
					      RESPONSE_FALLBACK);
		}

		/* Have the activation signal for the last entry field be
		 * equivalent to hitting the default button. */
		if (resp->last) {
			g_signal_connect(G_OBJECT(resp->last), "activate",
					 GTK_SIGNAL_FUNC(fake_respond_ok),
					 resp->dialog);
		}

		/* Show the dialog and grab focus. */
		gtk_widget_show_all(resp->dialog);
		if (GTK_IS_ENTRY(resp->first)) {
			gtk_widget_grab_focus(resp->first);
		}

		/* Run the dialog. */
		response = gtk_dialog_run(GTK_DIALOG(resp->dialog));

		/* Release the keyboard. */
		gdk_keyboard_ungrab(GDK_CURRENT_TIME);

		/* Answer the child's questions. */
		userhelper_write_childin(response, resp);

		/* Destroy the dialog box. */
		gtk_widget_destroy(resp->dialog);
		resp->table = NULL;
		resp->last = NULL;
		resp->first = NULL;
		resp->dialog = NULL;
		if (resp->title) {
			resp->title = NULL;
		}
		if (resp->banner) {
			g_free(resp->banner);
			resp->banner = NULL;
		}
		if (resp->suggestion) {
			g_free(resp->suggestion);
			resp->suggestion = NULL;
		}
		if (resp->service) {
			g_free(resp->service);
			resp->service = NULL;
		}
		if (resp->user) {
			g_free(resp->user);
			resp->user = NULL;
		}
		if (resp->message_list) {
			GList *e;

			for (e = g_list_first(resp->message_list); e != NULL;
			     e = g_list_next(e))
				g_free(e->data);
			g_list_free(resp->message_list);
			resp->message_list = NULL;
		}
		g_free(resp);
		resp = NULL;
	}
}

/* Handle a child-exited signal by disconnecting from its stdout. */
static void
userhelper_child_exited(GPid pid, int status, gpointer data)
{
	(void)data;
#ifdef DEBUG_USERHELPER
	fprintf(stderr, "Child %d exited (looking for %d).\n", pid, childpid);
#endif

	if (pid == childpid) {
#ifdef USE_STARTUP_NOTIFICATION
		/* If we're doing startup notification, clean it up just in
		 * case the child didn't complete startup. */
		if (sn_id != NULL) {
			userhelper_startup_notification_launchee(sn_id);
		}
#endif
		/* If we haven't lost the connection with the child, it's
		 * gone now. */
		if (childout_tag != 0) {
			g_source_remove(childout_tag);
			childout_tag = 0;
		}
		if (WIFEXITED(status)) {
#ifdef DEBUG_USERHELPER
			fprintf(stderr, "Child %d exited normally, ret = %d.\n",
				pid, WEXITSTATUS(status));
#endif
			userhelper_parse_exitstatus(WEXITSTATUS(status));
		} else {
#ifdef DEBUG_USERHELPER
			fprintf(stderr, "Child %d exited abnormally.\n", pid);
#endif
			if (WIFSIGNALED(status)) {
#ifdef DEBUG_USERHELPER
				fprintf(stderr, "Child %d died on signal %d.\n",
					pid, WTERMSIG(status));
#endif
				userhelper_parse_exitstatus(ERR_UNK_ERROR);
			}
		}
		userhelper_main_quit();
	}
}

/* Read data sent from the child userhelper process and pass it on to
 * userhelper_parse_childout(). */
static gboolean
userhelper_read_childout(GIOChannel *source, GIOCondition condition,
			 gpointer data)
{
	(void)data;
	if ((condition & (G_IO_ERR | G_IO_NVAL)) != 0) {
		/* Serious error, this is.  Panic, we must. */
		_exit(1);
	}
	if ((condition & G_IO_HUP) != 0) {
		/* EOF from the child. */
#ifdef DEBUG_USERHELPER
		g_print("EOF from child.\n");
#endif
		childout_tag = 0;
		return FALSE;
	}
	if ((condition & G_IO_IN) != 0) {
		char output[LINE_MAX + 1];
		gsize count;
		GError *err;

		err = NULL;
		g_io_channel_read_chars(source, output, LINE_MAX, &count, &err);
		if (err != NULL) {
			/* Error of some kind. */
			_exit(0);
		}
		/* Parse the data and we're done. */
		if (count > 0) {
			output[count] = '\0';
			userhelper_parse_childout(output);
		}
	}
	return TRUE;
}

int
userhelper_runv(gboolean dialog_success, const char *path, char **args)
{
	int childout[2];

	/* Create pipes with which to interact with the userhelper child. */
	if ((pipe(childout) == -1) || (pipe(childin) == -1)) {
		fprintf(stderr, _("Pipe error.\n"));
		_exit(1);
	}

	/* Start up a new process. */
	childpid = fork();
	if (childpid == -1) {
		fprintf(stderr, _("Cannot fork().\n"));
		_exit(0);
	}

	if (childpid > 0) {
		GIOChannel *childout_channel;
		GError *err;

		/* We're the parent; close the write-end of the reading pipe,
		 * and the read-end of the writing pipe. */
		close(childout[1]);
		close(childin[0]);

		/* Keep track of whether or not we need to display a dialog
		 * box on successful termination. */
		child_success_dialog = dialog_success;

		/* Watch the reading end of the reading pipe for data from the
		   child. */
		childout_channel = g_io_channel_unix_new(childout[0]);

		err = NULL;
		g_io_channel_set_flags(childout_channel, G_IO_FLAG_NONBLOCK,
				       &err);
		if (err != NULL) {
			fprintf(stderr,
				_("Failed to set IO channel nonblocking: %s\n"),
				err->message);
			_exit(1);
		}

		childout_tag = g_io_add_watch(childout_channel,
					      G_IO_IN | G_IO_HUP | G_IO_ERR
					      | G_IO_NVAL,
					      userhelper_read_childout, NULL);

		/* Watch for child exits. */
		child_exit_status = 0;
		g_child_watch_add(childpid, userhelper_child_exited, NULL);
#ifdef DEBUG_USERHELPER
		fprintf(stderr, "Running child pid=%ld.\n", (long) childpid);
#endif

		/* Tell the child we're ready for it to run. */
		write(childin[1], "Go", 1);
		gtk_main();

#ifdef USE_STARTUP_NOTIFICATION
		/* If we're doing startup notification, clean it up just in
		 * case the child didn't complete startup. */
		userhelper_startup_notification_launchee(sn_id);
#endif

#ifdef DEBUG_USERHELPER
		fprintf(stderr, "Child exited, continuing.\n");
#endif
	} else {
		int i, fd[4];
		unsigned char byte;
		long open_max;

		/* We're the child; close the read-end of the parent's reading
		 * pipe, and the write-end of the parent's writing pipe. */
		close(childout[0]);
		close(childin[1]);

		/* Read one byte from the parent so that we know it's ready
		 * to go. */
		read(childin[0], &byte, 1);

		/* Close all of descriptors which aren't stdio or the two
		 * pipe descriptors we'll be using. */
		open_max = sysconf(_SC_OPEN_MAX);
		for (i = 3; i < open_max; i++) {
			if ((i != childout[1]) && (i != childin[0])) {
				close(i);
			}
		}

		/* First create two copies of stdin, in case 3 and 4 aren't
		   currently in use.  STDIN_FILENO is surely valid even if the
		   user has started us with <&- because the X11 socket uses
		   a file descriptor. */
		fd[0] = dup(STDIN_FILENO);
		fd[1] = dup(STDIN_FILENO);

		/* Now create temporary copies of the pipe descriptors, which
		 * aren't goint to be 3 or 4 because they are surely in use
		 * by now. */
		fd[2] = dup(childin[0]);
		fd[3] = dup(childout[1]);

		/* Now get rid of the temporary descriptors, */
		close(fd[0]);
		close(fd[1]);
		close(childin[0]);
		close(childout[1]);

		/* and move the pipe descriptors to their new homes. */
		if (dup2(fd[2], UH_INFILENO) == -1) {
			fprintf(stderr, _("dup2() error.\n"));
			_exit(2);
		}
		if (dup2(fd[3], UH_OUTFILENO) == -1) {
			fprintf(stderr, _("dup2() error.\n"));
			_exit(2);
		}
		close(fd[2]);
		close(fd[3]);

#ifdef DEBUG_USERHELPER
		for (i = 0; args[i] != NULL; i++) {
			fprintf(stderr, "Exec arg %d = \"%s\".\n", i, args[i]);
		}
#endif
		execv(path, args);
		fprintf(stderr, _("execl() error, errno=%d\n"), errno);
		_exit(0);
	}
	return child_exit_status;
}

void
userhelper_run(gboolean dialog_success, const char *path, ...)
{
	va_list ap;
	char **argv;
	int argc = 0;
	int i = 0;

	/* Count the number of arguments. */
	va_start(ap, path);
	while (va_arg(ap, char *) != NULL) {
		argc++;
	}
	va_end(ap);

	/* Copy the arguments into a normal array. */
	argv = g_malloc((argc + 1) * sizeof(*argv));
	va_start(ap, path);
	for (i = 0; i < argc; i++)
		argv[i] = va_arg(ap, char *);
	argv[i] = NULL;
	va_end(ap);

	/* Pass the array into userhelper_runv() to actually run it. */
	userhelper_runv(dialog_success, path, argv);

	g_free(argv);
}
