/*
 * Copyright (C) 1997, 2001 Red Hat, Inc.
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

#include <sys/types.h>
#include <ctype.h>
#include <libintl.h>
#include <locale.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <gdk/gdkx.h>
#include "reaper.h"
#include "userdialogs.h"
#include "userhelper-wrap.h"

#define  PAD 8
static int childout[2];
static int childin[2];
static int childpid;
static int childout_tag = -1;

/* Call gtk_main_quit. */
void
userhelper_main_quit(void)
{
#ifdef DEBUG_USERHELPER
	fprintf(stderr, "Quitting main loop %d.\n", gtk_main_level());
#endif
	gtk_main_quit();
}

/* Display a dialog explaining a child's exit status, and exit ourselves. */
static void
userhelper_parse_exitstatus(int exitstatus)
{
	GtkWidget *message_box;
	int i;
	struct {
		int code;
		GtkWidget* (*create)(gchar*, gchar*);
		gchar *message;
	} codes[] = {
		{-1, create_error_box, 
		  _("Unknown exit code.")},
		{0, create_message_box,
		 _("Information updated.")},
		{ERR_PASSWD_INVALID, create_error_box,
		 _("The password you typed is invalid.\nPlease try again.")},
		{ERR_FIELDS_INVALID, create_error_box,
		 _("One or more of the changed fields is invalid.\nThis is probably due to either colons or commas in one of the fields.\nPlease remove those and try again.")},
		{ERR_SET_PASSWORD, create_error_box,
		 _("Password resetting error.")},
		{ERR_LOCKS, create_error_box,
		 _("Some systems files are locked.\nPlease try again in a few moments.")},
		{ERR_NO_USER, create_error_box,
		 _("Unknown user.")},
		{ERR_NO_RIGHTS, create_error_box,
		 _("Insufficient rights.")},
		{ERR_INVALID_CALL, create_error_box,
		 _("Invalid call to subprocess..")},
		{ERR_SHELL_INVALID, create_error_box,
		 _("Your current shell is not listed in /etc/shells.\nYou are not allowed to change your shell.\nConsult your system administrator.")},
		/* well, this is unlikely to work, but at least we tried... */
		{ERR_NO_MEMORY, create_error_box, 
		  _("Out of memory.")},
		{ERR_EXEC_FAILED, create_error_box, 
		  _("The exec() call failed.")},
		{ERR_NO_PROGRAM, create_error_box, 
		  _("Failed to find selected program.")},
		/* special no-display dialog */
		{ERR_CANCELED, NULL, ""},
		{ERR_UNK_ERROR, create_error_box, 
		  _("Unknown error.")},
	};

#ifdef DEBUG_USERHELPER
	fprintf(stderr, "Child returned exit status %d.\n", exitstatus);
#endif
	message_box = NULL;
	for (i = 1; i < G_N_ELEMENTS(codes); i++) {
		if (codes[i].code == exitstatus) {
			if (codes[i].create) {
				message_box = codes[i].create(codes[i].message,
							      NULL);
			}
			break;
		}
	}
	if (i >= G_N_ELEMENTS(codes)) {
		message_box = codes[0].create(codes[0].message, NULL);
	}

	if (message_box != NULL) {
		gtk_dialog_run(GTK_DIALOG(message_box));
		gtk_widget_destroy(message_box);
	}
}

/* Attempt to grab focus for the toplevel of this widget, so that peers can
 * get events too. */
static void
userhelper_grab_focus(GtkWidget *widget, GdkEventAny *event)
{
	int ret;
	ret = gdk_keyboard_grab(widget->window, TRUE, GDK_CURRENT_TIME);
	if (ret != 0) {
		g_warning("gdk_keyboard_grab returned %d", ret);
	}
}

/* Handle the executed dialog, writing responses back to the child when
 * possible. */
static void
userhelper_write_childin(GtkResponseType response, struct response *resp)
{
	const char *input;
	guchar byte;
	GList *message_list = resp->message_list;

	switch (response) {
		case PAD:
			/* Run unprivileged. */
			byte = UH_FALLBACK;
			for (message_list = resp->message_list;
			     (message_list != NULL) &&
			     (message_list->data != NULL);
			     message_list = g_list_next(message_list)) {
				message *m = (message *) message_list->data;
#ifdef DEBUG_USERHELPER
				fprintf(stderr, "message %d, \"%s\"\n", m->type,
					m->message);
				fprintf(stderr, "responding FALLBACK\n");
#endif
				if (GTK_IS_ENTRY(m->entry)) {
					write(childin[1], &byte, 1);
					write(childin[1], "\n", 1);
				}
			}
			break;
		case GTK_RESPONSE_CANCEL:
			/* Abort. */
			byte = UH_ABORT;
			for (message_list = resp->message_list;
			     (message_list != NULL) &&
			     (message_list->data != NULL);
			     message_list = g_list_next(message_list)) {
				message *m = (message *) message_list->data;
#ifdef DEBUG_USERHELPER
				fprintf(stderr, "message %d, \"%s\"\n", m->type,
					m->message);
				fprintf(stderr, "responding ABORT\n");
#endif
				if (GTK_IS_ENTRY(m->entry)) {
					write(childin[1], &byte, 1);
					write(childin[1], "\n", 1);
				}
			}
			break;
		case GTK_RESPONSE_OK:
			byte = UH_TEXT;
			for (message_list = resp->message_list;
			     (message_list != NULL) &&
			     (message_list->data != NULL);
			     message_list = g_list_next(message_list)) {
				message *m = (message *) message_list->data;
#ifdef DEBUG_USERHELPER
				fprintf(stderr, "message %d, \"%s\"\n", m->type,
					m->message);
				if (GTK_IS_ENTRY(m->entry)) {
					fprintf(stderr, "responding `%s'\n",
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
			break;
		default:
			/* We were closed, deleted, canceled, or something else
			 * which we can treat as a cancellation. */
			_exit(1);
			break;
	}
}

/* Glue. */
static void
respond_ok(GtkWidget *widget, GtkWidget *dialog)
{
	gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
}

/* Parse requests from the child userhelper process and display them in a
 * message box. */
static void
userhelper_parse_childout(char *outline)
{
	char *prompt;
	int prompt_type;
	struct response *resp = NULL;
	struct passwd *pwd;

	/* Attempt to reuse a response structure (which may contain incomplete
	 * messages we've already received) unless the dialog is bogus. */
	if (resp != NULL) {
		if (!GTK_IS_WINDOW(resp->dialog)) {
			g_free(resp->user);
			g_free(resp);
			resp = NULL;
		}
	}

	if (resp == NULL) {
		/* Allocate the response structure. */
		resp = g_malloc0(sizeof(struct response));

		/* Figure out who the invoking user is. */
		pwd = getpwuid(getuid());
		if (pwd == NULL) {
			pwd = getpwuid(0);
		}
		resp->user = pwd ? g_strdup(pwd->pw_name) : g_strdup("root");

		/* Create a table to hold the entry fields and labels. */
		resp->table = gtk_table_new(2, 1, FALSE);
	}

	/* Now process items from the child. */
	while ((outline != NULL) && isdigit(outline[0])) {
		gboolean echo = TRUE;

		/* Allocate a structure to hold the message data. */
		message *msg = g_malloc(sizeof(message));

		/* Read the prompt type. */
		prompt_type = strtol(outline, &prompt, 10);

		/* The first character which wasn't a digit might be whitespace,
		 * so skip over any whitespace before settling on the actual
		 * prompt. */
		if ((prompt != NULL) && (strlen(prompt) > 0)) {
			while ((isspace(prompt[0]) && (prompt[0] != '\0')
				&& (prompt[0] != '\n'))) {
				prompt++;
			}
		}

		/* Snip off terminating newlines in the prompt string and save
		 * a pointer to interate the parser along. */
		outline = strchr(prompt, '\n');
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
		msg->message = prompt;
		msg->data = NULL;
		msg->entry = NULL;

		echo = TRUE;
		switch(prompt_type) {
			/* Prompts.  Create a label and entry field. */
			case UH_ECHO_OFF_PROMPT:
				echo = FALSE;
				/* fall through */
			case UH_ECHO_ON_PROMPT:
				resp->title = _("Query");
				/* Create a label to hold the prompt. */
				msg->label = gtk_label_new(_(prompt));
				gtk_label_set_line_wrap(GTK_LABEL(msg->label),
							TRUE);
				gtk_misc_set_alignment(GTK_MISC(msg->label),
						       1.0, 0.5);

				/* Create an entry field to hold the answer. */
				msg->entry = gtk_entry_new();
				gtk_entry_set_visibility(GTK_ENTRY(msg->entry),
							 echo);
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

				/* Mark that this one needs a response. */
				resp->responses++;
				resp->rows++;
#ifdef DEBUG_USERHELPER
				g_print(_("Need %d responses.\n"),
					resp->responses);
#endif
				break;
			case UH_PROMPT_SUGGESTION:
				if (resp->suggestion) {
					g_free(resp->suggestion);
				}
				resp->suggestion = g_strdup(prompt);
				break;
			/* Fallback flag.  Read it and save it for later. */
			case UH_FALLBACK_ALLOW:
				resp->fallback_allowed = atoi(prompt) != 0;
				break;
			/* User name. Read it and save it for later. */
			case UH_USER:
				if (strstr(prompt, "<user>") == NULL) {
					if (resp->user) {
						g_free(resp->user);
					}
					resp->user = g_strdup(prompt);
				}
				break;
			/* Service name. Read it and save it for later. */
			case UH_SERVICE_NAME:
				if (resp->service) {
					g_free(resp->service);
				}
				resp->service = g_strdup(prompt);
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
				if (resp->banner != NULL) {
					g_free(resp->banner);
				}
				resp->banner = g_strdup(prompt);
				break;
			/* Sanity-check the number of expected responses. */
			case UH_EXPECT_RESP:
				g_free(msg); /* We don't need this after all. */
				if (resp->responses != atoi(prompt)) {
					fprintf(stderr,
						"Protocol error (%d responses "
						"expected from %d prompts)!\n",
						atoi(prompt), resp->responses);
					_exit(1);
				}
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

	/* If we're ready, do some last-minute changes and run the dialog. */
	if (resp->ready) {
		char *text;
		const char *imagefile = DATADIR "/pixmaps/keyring.png";
		GtkWidget *label, *image, *vbox;
		GtkResponseType response;

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
		gtk_window_set_title(GTK_WINDOW(resp->dialog),
				     resp->title ? resp->title : _("Error"));

		/* Force GTK+ to try to center this dialog. */
		gtk_window_set_position(GTK_WINDOW(resp->dialog),
					GTK_WIN_POS_CENTER_ALWAYS);

		/* If we're asking questions, change the dialog's icon. */
		if (resp->responses > 0) {
			image = (GTK_MESSAGE_DIALOG(resp->dialog))->image;
			gtk_image_set_from_file(GTK_IMAGE(image), imagefile);
		}

		/* Pack the table into the dialog box. */
		vbox = (GTK_DIALOG(resp->dialog))->vbox;
		gtk_box_pack_start_defaults(GTK_BOX(vbox), resp->table);

		/* Make sure we grab the keyboard focus when the window gets
		 * an X window associated with it. */
		gtk_signal_connect(GTK_OBJECT(resp->dialog), "map_event",
				   GTK_SIGNAL_FUNC(userhelper_grab_focus),
				   NULL);

		/* If the user closes the window, we bail. */
		gtk_signal_connect(GTK_OBJECT(resp->dialog), "delete_event",
				   GTK_SIGNAL_FUNC(userhelper_fatal_error),
				   NULL);

		/* Set the resp structure as the data item for the dialog. */
		gtk_object_set_user_data(GTK_OBJECT(resp->dialog), resp);

		/* Customize the label. */
		if (resp->responses == 0) {
			text = NULL;
		} else
		if (resp->service) {
			if (strcmp(resp->service, "passwd") == 0) {
				text = NULL;
			} else
			if (strcmp(resp->service, "chfn") == 0) {
				text = g_strdup_printf(_("Changing personal information."));
			} else
			if (strcmp(resp->service, "chsh") == 0) {
				text = g_strdup_printf(_("Changing login shell."));
			} else {
				if (resp->banner) {
					text = g_strdup(resp->banner);
				} else {
					if (resp->fallback_allowed) {
						text = g_strdup_printf(_("You are attempting to run \"%s\" which may benefit from administrative privileges, but more information is needed in order to do so."), resp->service);
					} else {
						text = g_strdup_printf(_("You are attempting to run \"%s\" which requires administrative privileges, but more information is needed in order to do so."), resp->service);
					}
				}
			}
		} else {
			if (resp->banner) {
				text = g_strdup(resp->banner);
			} else {
				if (resp->fallback_allowed) {
					text = g_strdup_printf(_("You are attempting to run a command which may benefit from administrative privileges, but more information is needed in order to do so."));
				} else {
					text = g_strdup_printf(_("You are attempting to run a command which requires administrative privileges, but more information is needed in order to do so."));
				}
			}
		}
		label = (GTK_MESSAGE_DIALOG(resp->dialog))->label;
		if (text != NULL) {
			gtk_label_set_text(GTK_LABEL(label), text);
			g_free(text);
			text = NULL;
		} else {
			gtk_label_set_text(GTK_LABEL(label), "");
		}

		/* Add an "unprivileged" button if we're allowed to offer
		 * unprivileged execution as an option. */
		if ((resp->fallback_allowed) && (resp->responses > 0)) {
			gtk_dialog_add_button(GTK_DIALOG(resp->dialog),
					      _("_Run Unprivileged"), PAD);
		}

		/* Have the activation signal for the last entry field be
		 * equivalent to hitting the default button. */
		if (resp->last) {
			g_signal_connect(G_OBJECT(resp->last), "activate",
					 GTK_SIGNAL_FUNC(respond_ok),
					 resp->dialog);
		}

		/* Show the dialog and grab focus. */
		gtk_widget_show_all(resp->dialog);
		if (GTK_IS_ENTRY(resp->first)) {
			gtk_widget_grab_focus(resp->first);
		}

		/* Run the dialog. */
		response = gtk_dialog_run(GTK_DIALOG(resp->dialog));
		userhelper_write_childin(response, resp);
		/* Release the keyboard. */
		gdk_keyboard_ungrab(GDK_CURRENT_TIME);
		/* Destroy the dialog box. */
		gtk_widget_destroy(resp->dialog);
		resp->dialog = NULL;
		if (resp->service)
			g_free(resp->service);
		if (resp->suggestion)
			g_free(resp->suggestion);
		if (resp->user)
			g_free(resp->user);
		resp->title = NULL;
		g_free(resp);
		resp = NULL;
	}
}

/* Handle a child-exited signal by disconnecting from its stdout. */
static void
userhelper_child_exited(VteReaper *reaper, guint pid, guint status,
			gpointer data)
{
#ifdef DEBUG_USERHELPER
	fprintf(stderr, "Child %d exited (looking for %d).\n", pid, childpid);
#endif
	if (pid == childpid) {
		if (childout_tag != -1) {
			g_source_remove(childout_tag);
		}
		if (WIFEXITED(status)) {
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
static void
userhelper_read_childout(gpointer data, int source, GdkInputCondition cond)
{
	char *output;
	int count;

	if (cond != GDK_INPUT_READ) {
		/* Serious error, this is.  Panic, we must. */
		_exit(1);
	}

	/* Allocate room to store the data, and store it. */
	output = g_malloc0(LINE_MAX + 1);
	count = read(source, output, LINE_MAX);
	if (count == -1) {
		/* Error of some kind.  Because the pipe's blocking, even
		 * EAGAIN is unexpected, so we bail. */
		_exit(0);
	}
	if (count == 0) {
		/* EOF from the child. */
		gdk_input_remove(childout_tag);
		childout_tag = -1;
	}

	/* Parse the data and we're done. */
	if (count > 0) {
		userhelper_parse_childout(output);
	}

	g_free(output);
}

void
userhelper_runv(char *path, const char **args)
{
	VteReaper *reaper;
	int retval;
	int i, fd[4];
	unsigned char byte;

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
		/* We're the parent; close the write-end of the reading pipe,
		 * and the read-end of the writing pipe. */
		close(childout[1]);
		close(childin[0]);

		/* Tell GDK to watch the reading end of the reading pipe for
		 * data from the child. */
		childout_tag = gdk_input_add(childout[0], GDK_INPUT_READ,
					     userhelper_read_childout, NULL);

		/* Watch for child exits. */
		reaper = vte_reaper_get();
		g_signal_connect(G_OBJECT(reaper), "child-exited",
				 G_CALLBACK(userhelper_child_exited),
				 NULL);
#ifdef DEBUG_USERHELPER
		fprintf(stderr, "Running child.\n");
#endif

		/* Tell the child we're ready for it to run. */
		write(childin[1], "Go", 1);
		gtk_main();
#ifdef DEBUG_USERHELPER
		fprintf(stderr, "Child exited, continuing.\n");
#endif
	} else {
		/* We're the child; close the read-end of the parent's reading
		 * pipe, and the write-end of the parent's writing pipe. */
		close(childout[0]);
		close(childin[1]);

		/* Read one byte from the parent so that we know it's ready
		 * to go. */
		read(childin[0], &byte, 1);

		/* Close all of descriptors which aren't stdio or the two
		 * pipe descriptors we'll be using. */
		for (i = 3; i < sysconf(_SC_OPEN_MAX); i++) {
			if ((i != childout[1]) && (i != childin[0])) {
				close(i);
			}
		}

		/* First create two copies of stdin, in case 3 and 4 aren't
		 * currently in use. */
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

		/* and move the pipe descriptors to their now homes. */
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
		retval = execv(path, (char**) args);
		fprintf(stderr, _("execl() error, errno=%d\n"), errno);
		_exit(0);
	}
}

void
userhelper_run(char *path, ...)
{
	va_list ap;
	const char **argv;
	int argc = 0;
	int i = 0;

	/* Count the number of arguments. */
	va_start(ap, path);
	while (va_arg(ap, char *) != NULL) {
		argc++;
	}
	va_end(ap);

	/* Copy the arguments into a normal array. */
	argv = g_malloc0((argc + 1) * sizeof(char*));
	va_start(ap, path);
	for (i = 0; i < argc; i++) {
		argv[i] = g_strdup(va_arg(ap, char *));
	}
	argv[i] = NULL;
	va_end(ap);

	/* Pass the array into userhelper_runv() to actually run it. */
	userhelper_runv(path, argv);

	g_free(argv);
}
