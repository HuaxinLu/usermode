/*
 * Copyright (C) 2002 Red Hat, Inc.  All rights reserved.
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

#include "gsmclient.h"
#include "eggtrayicon.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <libintl.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <gtk/gtk.h>

#define _(x) gettext (x)
#define N_(x) x

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
static EggTrayIcon *icon = NULL;
static GtkWidget *dialog = NULL;
static GtkWidget *start_dialog = NULL;

static void show_icon    (void);
static void hide_icon    (void);
static void launch_child (void);

static gboolean
child_io_func (GIOChannel   *source,
               GIOCondition  condition,
               void         *data)
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
  
  if ((condition & G_IO_HUP) ||
      (condition & G_IO_ERR) ||
      (condition & G_IO_NVAL))
    {
      respawn_child = TRUE;
      current_status = STATUS_UNKNOWN;
    }
  else if (condition & G_IO_IN)
    {
      GError *err;
      gsize i;
      gsize bytes_read;
      
      err = NULL;
      g_io_channel_read_chars (child_io_channel,
                               buf, sizeof (buf),
                               &bytes_read, &err);

      if (err != NULL)
        {
          g_printerr ("Error reading from pam_timestamp_check: %s\n",
                      err->message);
          g_error_free (err);

          respawn_child = TRUE;
          current_status = STATUS_UNKNOWN;
        }

      i = 0;
      while (i < bytes_read)
        {
          if (g_ascii_isdigit (buf[i]))
            output = atoi (buf + i);
          else if (buf[i] == '\n')
            ;
          else
            g_printerr ("Unknown byte '%d' from pam_timestamp_check\n",
                        (int) buf[i]);
          
          ++i;
        }
    }
  
  switch (output)
    {
    case 0:
      current_status = STATUS_AUTHENTICATED;
      break;
    case 1:
      current_status = STATUS_UNKNOWN;
      message = g_strdup ("bad args to pam_timestamp_check");
      break;
    case 2:
      message = g_strdup (_("pam_timestamp_check is not setuid root"));
      current_status = STATUS_BINARY_NOT_SUID;
      break;
    case 3:
      message = g_strdup (_("no controlling tty for pam_timestamp_check"));
      current_status = STATUS_NO_TTY;
      break;
    case 4:
      message = g_strdup (_("user unknown to pam_timestamp_check"));
      current_status = STATUS_USER_UNKNOWN;
      break;
    case 5:
      message = g_strdup (_("permissions error in pam_timestamp_check"));
      current_status = STATUS_PERMISSIONS_ERROR;
      break;
    case 6:
      message = g_strdup (_("invalid controlling tty in pam_timestamp_check"));
      current_status = STATUS_INVALID_TTY;
      break;
      
    case 7:
      /* timestamp just isn't held - user hasn't authenticated */
      current_status = STATUS_OTHER_ERROR;
      break;

    default:
      message = g_strdup ("got unknown code from pam_timestamp_check");
      current_status = STATUS_UNKNOWN;
      break;      
    }

  if (message)
    {
      /*  FIXME, dialog? */
      if (old_status != current_status)
        g_printerr (_("Error: %s\n"), message);
      
      g_free (message);
    }
  
  exit_status = 0;
  retval = waitpid (child_pid, &exit_status, WNOHANG);

  if (retval < 0)
    {
      g_printerr ("Failed in waitpid(): %s\n", g_strerror (errno));
    }
  else if (retval == 0)
    {
      /* No child has exited */
    }
  else if (retval == child_pid)
    {
      /* Child has exited */
      current_status = STATUS_UNKNOWN;
      respawn_child = TRUE;

      if (WIFSIGNALED (exit_status))
        g_printerr ("pam_timestamp_check died on signal %d\n",
                    WTERMSIG (exit_status));
    }
  else
    {
      g_printerr ("Confused about waitpid(): returned %d, child pid was %d\n",
                  retval, child_pid);
    }

  if (current_status == STATUS_AUTHENTICATED)
    show_icon ();
  else
    hide_icon ();
  
  if (respawn_child)
    {
      if (child_io_channel != NULL)
        {
          g_io_channel_unref (child_io_channel);
          child_io_channel = NULL;
          child_pid = -1;
        }

      /* Respawn the child */
      launch_child ();
      
      return FALSE;
    }
  else
    {
      return TRUE;
    }
}

static void
session_save_callback (GsmClient *client,
                       gboolean   is_phase2,
                       void      *data)
{
  const char *argv[4];

#define ARGC 3
  argv[0] = "pam-panel-icon";
  argv[1] = "--sm-client-id";
  argv[2] = gsm_client_get_id (client);
  argv[3] = NULL;

  gsm_client_set_restart_command (client,
                                  ARGC, (char**) argv);
  
  argv[1] = NULL;
  gsm_client_set_clone_command (client,
                                1, (char**) argv);
}

static void
session_die_callback (GsmClient *client,
                      void      *data)
{
  exit (1);
}

int
main (int argc, char **argv)
{
  GMainLoop *loop;
  GsmClient *client;
  const char *previous_id;

  previous_id = NULL;
  
  gtk_init (&argc, &argv);

  if (argc > 1)
    {
      if (argc != 3 ||
          strcmp (argv[1], "--sm-client-id") != 0)
        {
          g_printerr ("pam-panel-icon: invalid args\n");
          return 1;
        }

      previous_id = argv[2];
    }
  
  client = gsm_client_new ();

  gsm_client_set_restart_style (client, GSM_RESTART_IMMEDIATELY);
  /* start up last */
  gsm_client_set_priority (client, GSM_CLIENT_PRIORITY_NORMAL + 10);
  
  gsm_client_connect (client, previous_id);

  if (!gsm_client_get_connected (client))
    g_printerr (_("pam-panel-icon: failed to connect to session manager\n"));

  g_signal_connect (G_OBJECT (client), "save",
                    G_CALLBACK (session_save_callback),
                    NULL);

  g_signal_connect (G_OBJECT (client), "die",
                    G_CALLBACK (session_die_callback),
                    NULL);
  
  loop = g_main_loop_new (NULL, FALSE);

  launch_child ();
  
  g_main_loop_run (loop);

  g_main_loop_unref (loop);
  
  return 0;
}

static void
dialog_response_cb (GtkWidget *dialog,
                    int        response_id,
                    void      *data)
{
  gtk_widget_destroy (dialog);
  
  if (response_id == GTK_RESPONSE_ACCEPT)
    {
      GError *err;
      int exit_status;
      char *argv[] = { "/sbin/pam_timestamp_check", "-k", "root", NULL };
      
      exit_status = 0;
      err = NULL;
      if (!g_spawn_sync ("/",
                         argv, NULL, G_SPAWN_CHILD_INHERITS_STDIN,
                         NULL, NULL, NULL, NULL,
                         &exit_status,
                         &err))
        {
          GtkWidget *d;
          
          d = gtk_message_dialog_new (NULL,
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_ERROR,
                                      GTK_BUTTONS_CLOSE,
                                      _("Failed to drop administrator privileges: %s"),
                                      err->message);
          g_signal_connect (G_OBJECT (d), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);
          
          gtk_window_set_resizable (GTK_WINDOW (d), FALSE);
          
          gtk_widget_show (d);
          
          g_error_free (err);
        }
      else
        {
          if (WIFEXITED (exit_status) && WEXITSTATUS (exit_status) != 0)
            {
              GtkWidget *d;
              
              d = gtk_message_dialog_new (NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_MESSAGE_ERROR,
                                          GTK_BUTTONS_CLOSE,
                                          _("Failed to drop administrator privileges: "
                                            "pam_timestamp_check returned failure code %d"),
                                          WEXITSTATUS (exit_status));

              g_signal_connect (G_OBJECT (d), "response",
                                G_CALLBACK (gtk_widget_destroy),
                                NULL);
              
              gtk_window_set_resizable (GTK_WINDOW (d), FALSE);
              
              gtk_widget_show (d);
            }
        }
    }
}

static gboolean
icon_clicked_event (GtkWidget      *widget,
                    GdkEventButton *event,
                    void           *data)
{
  if (event->button != 1)
    return FALSE;

  if (dialog != NULL)
    {
      gtk_window_present (GTK_WINDOW (dialog));
      return TRUE;
    }
  
  dialog = gtk_message_dialog_new (NULL,
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_QUESTION,
                                   GTK_BUTTONS_NONE,
                                   _("You're currently authorized to configure systemwide settings (that affect all users) without typing the administrator password again. You can give up this authorization."));

  g_object_add_weak_pointer (G_OBJECT (dialog), (void**) &dialog);

  gtk_dialog_add_button (GTK_DIALOG (dialog),
                         _("Keep Authorization"), GTK_RESPONSE_REJECT);

  gtk_dialog_add_button (GTK_DIALOG (dialog),
                         _("Forget Password"), GTK_RESPONSE_ACCEPT);
  
  g_signal_connect (G_OBJECT (dialog), "response",
                    G_CALLBACK (dialog_response_cb),
                    NULL);
  
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  
  gtk_window_present (GTK_WINDOW (dialog));
  
  return TRUE;
}

static void
show_icon (void)
{
  if (icon == NULL)
    {
      GtkWidget *image;
      
      icon = egg_tray_icon_new ("Authentication Indicator");

      /* If the system tray goes away, our icon can get destroyed */
      g_object_add_weak_pointer (G_OBJECT (icon), (void**) &icon);
      
      if (g_file_test ("./status_lock.png", G_FILE_TEST_EXISTS))
        image = gtk_image_new_from_file ("./status_lock.png");
      else
        image = gtk_image_new_from_file (DATADIR"/pixmaps/status_lock.png");

      gtk_container_add (GTK_CONTAINER (icon), image);
      gtk_widget_show (image);

      gtk_widget_add_events (GTK_WIDGET (icon), GDK_BUTTON_PRESS_MASK);
      g_signal_connect (G_OBJECT (icon), "button_press_event",
                        G_CALLBACK (icon_clicked_event), NULL);
    }

  /* When the icon first appears show a dialog */
  if (icon && !GTK_WIDGET_VISIBLE (icon))
    {
      if (start_dialog == NULL)
        {
          start_dialog =
            gtk_message_dialog_new (NULL,
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_INFO,
                                    GTK_BUTTONS_CLOSE,
                                    _("You are now authorized to modify system settings that affect all users of this computer.\nThis authorization will remain active for a short time."));

          g_object_add_weak_pointer (G_OBJECT (start_dialog),
                                     (void**) &start_dialog);
          
          gtk_window_set_resizable (GTK_WINDOW (start_dialog), FALSE);

          g_signal_connect (G_OBJECT (start_dialog), "response",
                            G_CALLBACK (gtk_widget_destroy),
                            NULL);
        }

      gtk_window_present (GTK_WINDOW (start_dialog));
    }
  
  gtk_widget_show (GTK_WIDGET (icon));
  
  /* g_print ("showing icon status = %d child_pid = %d\n", current_status, child_pid); */
}

static void
hide_icon (void)
{
  if (icon != NULL)
    {
      gtk_widget_destroy (GTK_WIDGET (icon));
      icon = NULL;
    }

  if (start_dialog != NULL)
    gtk_widget_destroy (start_dialog);

  /*   g_print ("hiding icon status = %d child_pid = %d\n", current_status, child_pid); */
}

static void
launch_child (void)
{
  GError *err;
  char *command[] = { "/sbin/pam_timestamp_check",  "-d", "root", NULL };
  int out_fd;

  if (child_io_channel != NULL)
    return;
  
  /* have to inherit stdin so pam_timestamp_check can get at the tty */
  out_fd = -1;
  err = NULL;
  if (!g_spawn_async_with_pipes ("/", command, NULL,
                                 G_SPAWN_CHILD_INHERITS_STDIN |
                                 G_SPAWN_DO_NOT_REAP_CHILD,
                                 NULL, NULL, &child_pid, NULL, &out_fd, NULL,
                                 &err))
    {
      g_printerr (_("Failed to run command \"%s\": %s\n"),
                  command[0], err->message);

      g_error_free (err);

      return;
    }

  child_io_channel = g_io_channel_unix_new (out_fd);
  err = NULL;
  g_io_channel_set_flags (child_io_channel, G_IO_FLAG_NONBLOCK, &err);
  if (err != NULL)
    {
      g_printerr (_("Failed to set IO channel nonblocking: %s\n"),
                  err->message);
      g_error_free (err);

      child_pid = -1;
      g_io_channel_unref (child_io_channel);
      child_io_channel = NULL;
      return;
    }
  
  g_io_add_watch (child_io_channel, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                  child_io_func, NULL);
}
