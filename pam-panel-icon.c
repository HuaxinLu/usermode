/*
 * Copyright (C) 2002,2003 Red Hat, Inc.  All rights reserved.
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
#include <sys/wait.h>
#include <errno.h>
#include <libintl.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gsmclient.h"
#include "eggtrayicon.h"

#define _(String) gettext(String)
#define N_(String) String
#define PAM_TIMESTAMP_CHECK_PATH "/sbin/pam_timestamp_check"

enum {
	RESPONSE_DROP = 0,
	RESPONSE_DO_NOTHING = -1
};

enum {
	STATUS_UNKNOWN = -1,
	STATUS_AUTHENTICATED = 0,
	STATUS_BINARY_NOT_SUID = 2,
	STATUS_NO_TTY = 3,
	STATUS_USER_UNKNOWN = 4,
	STATUS_PERMISSIONS_ERROR = 5,
	STATUS_INVALID_TTY = 6,
	STATUS_OTHER_ERROR = 7
};

static int current_status = STATUS_UNKNOWN;
static GIOChannel *child_io_channel = NULL;
static pid_t child_pid = -1;
static EggTrayIcon *tray_icon = NULL;
static GtkWidget *image = NULL;
static GtkWidget *drop_dialog = NULL;
static GdkPixbuf *locked_pixbuf = NULL;
static pid_t running_init_pid = -1;

static void launch_checker(void);

/* Respond to a button press in the drop dialog. */
static void
drop_dialog_response_cb(GtkWidget *dialog, int response_id, void *data)
{
	GError *err;
	int exit_status;
	char *argv[4];

	gtk_widget_destroy(dialog);

	if (response_id == RESPONSE_DROP) {
		argv[0] = PAM_TIMESTAMP_CHECK_PATH;
		argv[1] = "-k";
		argv[2] = "root";
		argv[3] = NULL;

		exit_status = 0;
		err = NULL;
		if (!g_spawn_sync("/",
				  argv, NULL, G_SPAWN_CHILD_INHERITS_STDIN,
				  NULL, NULL, NULL, NULL,
				  &exit_status, &err)) {
			/* There was an error running the command. */
			dialog = gtk_message_dialog_new(NULL,
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_ERROR,
							GTK_BUTTONS_CLOSE,
							_("Failed to drop administrator privileges: %s"),
							err->message);
			g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(gtk_widget_destroy),
					 NULL);
			gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
			gtk_widget_show(dialog);

			g_error_free(err);
		} else {
			if (WIFEXITED(exit_status) &&
			    (WEXITSTATUS(exit_status) != 0)) {
				dialog = gtk_message_dialog_new(NULL,
								GTK_DIALOG_DESTROY_WITH_PARENT,
								GTK_MESSAGE_ERROR,
								GTK_BUTTONS_CLOSE,
								_("Failed to drop administrator privileges: "
								  "pam_timestamp_check returned failure code %d"),
								WEXITSTATUS(exit_status));
				g_signal_connect(G_OBJECT(dialog), "response",
						 G_CALLBACK(gtk_widget_destroy),
						 NULL);
				gtk_window_set_resizable(GTK_WINDOW(dialog),
							 FALSE);
				gtk_window_present(GTK_WINDOW(dialog));
			}
		}
	}
}

static void
add_weak_widget_pointer(GObject *object, GtkWidget **weak_pointer)
{
	g_object_add_weak_pointer(object, (void**)weak_pointer);
}
static void
add_weak_egg_tray_icon_pointer(GObject *object, EggTrayIcon **weak_pointer)
{
	g_object_add_weak_pointer(object, (void**)weak_pointer);
}

/* Respond to a button press event on our icon. */
static gboolean
icon_clicked_event(GtkWidget *widget, GdkEventButton *event, void *data)
{
	/* We only respond to left-click. */
	if (event->button != 1) {
		return FALSE;
	}

	/* If we're already authenticated, give the user the option of removing
	 * the timestamp. */
	if (current_status == STATUS_AUTHENTICATED) {
		/* If there's not already a dialog up, create one. */
		if (drop_dialog == NULL) {
			drop_dialog = gtk_message_dialog_new(NULL,
							     GTK_DIALOG_DESTROY_WITH_PARENT,
							     GTK_MESSAGE_QUESTION,
							     GTK_BUTTONS_NONE,
							     _("You're currently authorized to configure system-wide settings (that affect all users) without typing the administrator password again. You can give up this authorization."));

			add_weak_widget_pointer(G_OBJECT(drop_dialog),
						&drop_dialog);

			gtk_dialog_add_button(GTK_DIALOG(drop_dialog),
					      _("Keep Authorization"),
					      RESPONSE_DO_NOTHING);

			gtk_dialog_add_button(GTK_DIALOG(drop_dialog),
					      _("Forget Authorization"),
					      RESPONSE_DROP);

			g_signal_connect(G_OBJECT(drop_dialog), "response",
					 G_CALLBACK(drop_dialog_response_cb),
					 NULL);

			gtk_window_set_resizable(GTK_WINDOW(drop_dialog),
						 FALSE);
		}
		gtk_window_present(GTK_WINDOW(drop_dialog));
	}
	return TRUE;
}

static void
ensure_tray_icon(void)
{
	if (tray_icon == NULL) {
		tray_icon = egg_tray_icon_new("Authentication Indicator");
		image = gtk_image_new();

		/* If the system tray goes away, our icon will get destroyed,
		 * and we don't want to be left with a dangling pointer to it
		 * if that happens.  */
		add_weak_egg_tray_icon_pointer(G_OBJECT(tray_icon), &tray_icon);
		add_weak_widget_pointer(G_OBJECT(image), &image);

		/* Add the image to the icon. */
		gtk_container_add(GTK_CONTAINER(tray_icon), image);
		gtk_widget_show(image);

		/* Handle clicking events. */
		gtk_widget_add_events(GTK_WIDGET(tray_icon),
				      GDK_BUTTON_PRESS_MASK);
		g_signal_connect(G_OBJECT(tray_icon), "button_press_event",
				 G_CALLBACK(icon_clicked_event), NULL);
	}

	gtk_widget_show(GTK_WIDGET(tray_icon));
}

static void
show_unlocked_icon(void)
{
	ensure_tray_icon();
	gtk_image_set_from_pixbuf(GTK_IMAGE(image), NULL);
	gtk_widget_hide(image);
}

static void
show_locked_icon(void)
{
	ensure_tray_icon();
	gtk_image_set_from_pixbuf(GTK_IMAGE(image), locked_pixbuf);
	gtk_widget_show(image);
}

static gboolean
child_io_func(GIOChannel *source, GIOCondition condition, void *data)
{
	int exit_status;
	int retval;
	gboolean respawn_child;
	int output;
	char *message;
	int old_status;
	char buf[10];

	message = NULL;
	output = 0;
	respawn_child = FALSE;
	old_status = current_status;

	/* While we're here, let's check the status of the running_init child */
	if (running_init_pid != -1) {
		retval = waitpid(running_init_pid, &exit_status, WNOHANG);
		if (retval != 0) {
			if (retval == running_init_pid) {
				running_init_pid = -1;
			} else {
				g_printerr("Confused about waitpid(): %s\n"
					   "returned %d, child pid was %d\n",
					   g_strerror(errno), retval,
					   running_init_pid);
				running_init_pid = -1;
			}
		}
	}


	if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
		/* Error conditions mean we need to launch a new child. */
		respawn_child = TRUE;
		current_status = STATUS_UNKNOWN;
	} else
	if (condition & G_IO_IN) {
		GError *err;
		gsize i;
		gsize bytes_read;

		err = NULL;
		g_io_channel_read_chars(child_io_channel,
					buf, sizeof(buf),
					&bytes_read, &err);

		if (err != NULL) {
			g_printerr("Error reading from pam_timestamp_check: "
				   "%s\n", err->message);
			g_error_free(err);

			respawn_child = TRUE;
			current_status = STATUS_UNKNOWN;
		}

		for (i = 0; i < bytes_read; i++) {
			if (!g_ascii_isdigit(buf[i]) && (buf[i] != '\n')) {
				g_printerr("Unknown byte '%d' from "
					   "pam_timestamp_check.\n",
					   (int) buf[i]);
			}
			if (g_ascii_isdigit(buf[i])) {
				output = atoi(buf + i);
			}
		}
	}

	switch (output) {
	case 0:
		current_status = STATUS_AUTHENTICATED;
		break;
	case 1:
		current_status = STATUS_UNKNOWN;
		message = g_strdup("bad args to pam_timestamp_check");
		break;
	case 2:
		message = g_strdup(_("pam_timestamp_check is not setuid root"));
		current_status = STATUS_BINARY_NOT_SUID;
		break;
	case 3:
		message =
		    g_strdup(_("no controlling tty for pam_timestamp_check"));
		current_status = STATUS_NO_TTY;
		break;
	case 4:
		message = g_strdup(_("user unknown to pam_timestamp_check"));
		current_status = STATUS_USER_UNKNOWN;
		break;
	case 5:
		message = g_strdup(_("permissions error in pam_timestamp_check"));
		current_status = STATUS_PERMISSIONS_ERROR;
		break;
	case 6:
		message = g_strdup(_("invalid controlling tty in pam_timestamp_check"));
		current_status = STATUS_INVALID_TTY;
		break;

	case 7:
		/* timestamp just isn't held - user hasn't authenticated */
		current_status = STATUS_OTHER_ERROR;
		break;

	default:
		message = g_strdup("got unknown code from pam_timestamp_check");
		current_status = STATUS_UNKNOWN;
		break;
	}

	if (message) {
		/*  FIXME, dialog? */
		if (old_status != current_status) {
			g_printerr(_("Error: %s\n"), message);
		}
		g_free(message);
	}

	exit_status = 0;
	retval = waitpid(child_pid, &exit_status, WNOHANG);

	if (retval < 0) {
		g_printerr("Failed in waitpid(): %s\n", g_strerror(errno));
	} else
	if (retval == 0) {
		/* No child has exited */
	} else
	if (retval == child_pid) {
		/* Child has exited */
		current_status = STATUS_UNKNOWN;
		respawn_child = TRUE;

		if (WIFSIGNALED(exit_status)) {
			g_printerr("pam_timestamp_check died on signal %d\n",
				   WTERMSIG(exit_status));
		}
	} else {
		g_printerr("Confused about waitpid(): returned %d, child pid "
			   "was %d\n", retval, child_pid);
	}

	if (current_status == STATUS_AUTHENTICATED) {
		show_locked_icon();
	} else {
		show_unlocked_icon();
	}

	if (respawn_child) {
		if (child_io_channel != NULL) {
			g_io_channel_unref(child_io_channel);
			child_io_channel = NULL;
			child_pid = -1;
		}

		/* Respawn the child */
		launch_checker();

		return FALSE;
	} else {
		return TRUE;
	}
}

/* Launch the child which checks for timestamps. */
static void
launch_checker(void)
{
	GError *err;
	char *command[] = {
		PAM_TIMESTAMP_CHECK_PATH,
		"-d",
		"root",
		NULL
	};
	int out_fd;

	/* Don't launch the checker more than once. */
	if (child_io_channel != NULL) {
		return;
	}

	/* Let the child inherit stdin so that pam_timestamp_check can get at
	 * the panel's controlling tty, if there is one. */
	out_fd = -1;
	err = NULL;
	if (!g_spawn_async_with_pipes("/", command, NULL,
				      G_SPAWN_CHILD_INHERITS_STDIN |
				      G_SPAWN_DO_NOT_REAP_CHILD,
				      NULL, NULL, &child_pid, NULL,
				      &out_fd, NULL, &err)) {
		g_printerr(_("Failed to run command \"%s\": %s\n"),
			   command[0], err->message);
		g_error_free(err);
		return;
	}

	/* We're watching for output from the child. */
	child_io_channel = g_io_channel_unix_new(out_fd);

	/* Try to set the channel non-blocking. */
	err = NULL;
	g_io_channel_set_flags(child_io_channel, G_IO_FLAG_NONBLOCK, &err);
	if (err != NULL) {
		g_printerr(_("Failed to set IO channel nonblocking: %s\n"),
			   err->message);
		g_error_free(err);

		child_pid = -1;
		g_io_channel_unref(child_io_channel);
		child_io_channel = NULL;
		return;
	}

	/* Set up a callback for when the child tells us something. */
	g_io_add_watch(child_io_channel,
		       G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
		       child_io_func, NULL);
}

/* Save ourselves. */
static void
session_save_callback(GsmClient *client, gboolean is_phase2, void *data)
{
	const char *argv[4];

	argv[0] = BINDIR "/pam-panel-icon";
	argv[1] = "--sm-client-id";
	argv[2] = gsm_client_get_id(client);
	argv[3] = NULL;

	/* The restart command will restart us with this same session ID. */
	gsm_client_set_restart_command(client, G_N_ELEMENTS(argv) - 1,
				       (char**) argv);

	/* The clone command will start up another copy. */
	gsm_client_set_clone_command(client, 1, (char**) argv);
}

/* Handle the "session closing" notification. */
static void
session_die_callback(GsmClient *client, void *data)
{
	gtk_main_quit();
}

int
main(int argc, char **argv)
{
	GsmClient *client = NULL;
	const char *previous_id = NULL;

	gtk_init(&argc, &argv);

	/* Check if a session management ID was passed on the command line. */
	if (argc > 1) {
		if ((argc != 3) || (strcmp(argv[1], "--sm-client-id") != 0)) {
			g_printerr("pam-panel-icon: invalid args\n");
			return 1;
		}
		previous_id = argv[2];
	}

	/* Load the images. */
	locked_pixbuf = gdk_pixbuf_new_from_file(DATADIR
						 "/pixmaps/keyring-small.png",
						 NULL);

	/* Start up locales */
        setlocale(LC_ALL, "");
        bindtextdomain(PACKAGE, DATADIR "/locale");
        bind_textdomain_codeset(PACKAGE, "UTF-8");
        textdomain(PACKAGE);

	client = gsm_client_new();

	gsm_client_set_restart_style(client, GSM_RESTART_IMMEDIATELY);
	/* start up last */
	gsm_client_set_priority(client, GSM_CLIENT_PRIORITY_NORMAL + 10);

	gsm_client_connect(client, previous_id);

	if (!gsm_client_get_connected(client)) {
		g_printerr(_("pam-panel-icon: failed to connect to session manager\n"));
	}

	g_signal_connect(G_OBJECT(client), "save",
			 G_CALLBACK(session_save_callback), NULL);

	g_signal_connect(G_OBJECT(client), "die",
			 G_CALLBACK(session_die_callback), NULL);

	launch_checker();

	gtk_main();

	return 0;
}
