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

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "userhelper-wrap.h"
#include "userdialogs.h"

#define MAXLINE 512

int childout[2];
int childin[2];
int childout_tag;

void *
userhelper_malloc(size_t size) {
  void *ret;

  ret = malloc(size);
  if (!ret) exit (ERR_NO_MEMORY);

  return ret;
}

void
userhelper_runv(char *path, char **args)
{
  pid_t pid;
  int retval;

  if(pipe(childout) < 0 || pipe(childin) < 0)
    {
      fprintf(stderr, "Pipe error.\n");
      exit(1);
    }

  if((pid = fork()) < 0)
    {
      fprintf(stderr, "Cannot fork().\n");
    }
  else if(pid > 0)		/* parent */
    {
      close(childout[1]);
      close(childin[0]);

      childout_tag = gdk_input_add(childout[0], GDK_INPUT_READ, (GdkInputFunction) userhelper_read_childout, NULL);

    }
  else				/* child */
    {
      close(childout[0]);
      close(childin[1]);

      if(childout[1] != STDOUT_FILENO)
	{
	  if(dup2(childout[1], STDOUT_FILENO) != STDOUT_FILENO)
	    {
	      fprintf(stderr, "dup2() error.\n");
	      exit(2);
	    }
	  close(childout[1]);
	}
      if(childin[0] != STDIN_FILENO)
	{
	  if(dup2(childin[0], STDIN_FILENO) != STDIN_FILENO)
	    {
	      fprintf(stderr, "dup2() error.\n");
	      exit(2);
	    }
	}
      setbuf(stdout, NULL);

      retval = execv(path, args);

      if(retval < 0) {
	fprintf(stderr, "execl() error, errno=%d\n", errno);
      }

      _exit(0);

    }
}
void
userhelper_run(char *path, ...)
{
  va_list ap;
  char *args[256]; /* only used internally, we know this will not overflow */
  int i = 0;

  va_start(ap, path);
  do {
    args[i++] = va_arg(ap, char *);
  } while (args[i]);
  va_end(ap);

  userhelper_runv(path, args);
}

void
userhelper_parse_exitstatus(int exitstatus)
{
  GtkWidget* message_box;

  switch(exitstatus)
    {
    case 0:
      message_box = create_message_box("Information updated.", NULL);
      break;
    case ERR_PASSWD_INVALID:
      message_box = create_error_box("The password you typed is invalid.\nPlease try again.", NULL);
      break;
    case ERR_FIELDS_INVALID:
      message_box = create_error_box("One or more of the changed fields is invalid.\nThis is probably due to either colons or commas in one of the fields.\nPlease remove those and try again.", NULL);
      break;
    case ERR_SET_PASSWORD:
      message_box = create_error_box("Password resetting error.", NULL);
      break;
    case ERR_LOCKS:
      message_box = create_error_box("Some systems files are locked.\nPlease try again in a few moments.", NULL);
      break;
    case ERR_NO_USER:
      message_box = create_error_box("Unknown user.", NULL);
      break;
    case ERR_NO_RIGHTS:
      message_box = create_error_box("Insufficient rights.", NULL);
      break;
    case ERR_INVALID_CALL:
      message_box = create_error_box("Invalid call to sub process.", NULL);
      break;
    case ERR_SHELL_INVALID:
      message_box = create_error_box("Your current shell is not listed in /etc/shells.\nYou are not allowed to change your shell.\nConsult your system administrator.", NULL);
      break;
    case ERR_NO_MEMORY:
      /* well, this is unlikely to work either, but at least we tried... */
      message_box = create_error_box("Out of memory.", NULL);
      break;
    case ERR_EXEC_FAILED:
      message_box = create_error_box("The exec() call failed.", NULL);
      break;
    case ERR_NO_PROGRAM:
      message_box = create_error_box("Failed to find selected program.", NULL);
      break;
    case ERR_UNK_ERROR:
      message_box = create_error_box("Unknown error.", NULL);
      break;
    default:
      message_box = create_error_box("Unknown exit code.", NULL);
      break;
    }

  gtk_signal_connect(GTK_OBJECT(message_box), "destroy", (GtkSignalFunc) userhelper_fatal_error, NULL);
  gtk_signal_connect(GTK_OBJECT(message_box), "delete_event", (GtkSignalFunc) userhelper_fatal_error, NULL);
  gtk_widget_show(message_box);

}

void
userhelper_grab_focus(GtkWidget *widget, GdkEvent *map_event, gpointer data)
{
	int ret;
	GtkWidget *toplevel;
	/* grab focus for the toplevel of this widget, so that peers can
	 * get events too */
	toplevel = gtk_widget_get_toplevel(widget);
	ret = gdk_keyboard_grab(toplevel->window, TRUE, GDK_CURRENT_TIME);
}

void
userhelper_parse_childout(char* outline)
{
  char* prompt;
  char* current;
  char* rest = NULL;
  int prompt_type;
  static response *resp = NULL; /* this will be attached to the toplevel */

  prompt = strchr(outline, ' ');

  if(prompt == NULL) return;

  if (!resp) {
    resp = userhelper_malloc(sizeof(response));

    resp->num_components = 1;
    resp->message_list = NULL;
    resp->head = resp->tail = NULL;
    resp->top = gtk_dialog_new();
    gtk_signal_connect(GTK_OBJECT(resp->top), "map",
		       GTK_SIGNAL_FUNC(userhelper_grab_focus), NULL);
    gtk_window_position(GTK_WINDOW(resp->top), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(resp->top), 5);
    gtk_window_set_title(GTK_WINDOW(resp->top), "Input");
    resp->ok = gtk_button_new_with_label(UD_OK_TEXT);
    gtk_misc_set_padding(GTK_MISC(GTK_BIN(resp->ok)->child), 4, 0);
    resp->cancel = gtk_button_new_with_label(UD_CANCEL_TEXT);
    gtk_misc_set_padding(GTK_MISC(GTK_BIN(resp->cancel)->child), 4, 0);
    resp->table = gtk_table_new(1, 2, FALSE);

    gtk_box_set_homogeneous(GTK_BOX(GTK_DIALOG(resp->top)->action_area), TRUE);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(resp->top)->action_area),
	resp->ok, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(resp->top)->action_area),
	resp->cancel, TRUE, TRUE, 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(resp->top)->vbox),
	resp->table, FALSE, FALSE, 5);

    gtk_signal_connect(GTK_OBJECT(resp->top), "delete_event", 
		       (GtkSignalFunc) userhelper_fatal_error, NULL);
    gtk_signal_connect(GTK_OBJECT(resp->cancel), "clicked", 
		       (GtkSignalFunc) userhelper_fatal_error, NULL);

    gtk_signal_connect(GTK_OBJECT(resp->ok), "clicked", 
		       (GtkSignalFunc) userhelper_write_childin, resp);

    gtk_object_set_user_data(GTK_OBJECT(resp->top), resp);
  }

  current = prompt - 1;
  if(isdigit(current[0])) {
    message *msg = userhelper_malloc(sizeof(message));

    current[1] = '\0';
    prompt_type = atoi(current);

    prompt++;
    /* null terminate the actual prompt... then call this function
     * on the rest
     */
    current = prompt;

    rest = strchr(current, '\n');
    if(rest) {
      *rest = '\0';
      rest++;
      if (!*rest) {
	rest = NULL;
      }
    }

    msg->type = prompt_type;
    msg->message = prompt;
    msg->data = NULL;
    msg->entry = NULL;

    switch(prompt_type) {
      case UH_ECHO_OFF_PROMPT:
	msg->entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(msg->entry), FALSE);
	/* fall through */
      case UH_ECHO_ON_PROMPT:
	if (!msg->entry) msg->entry = gtk_entry_new();
	msg->label = gtk_label_new(prompt);
	gtk_misc_set_alignment(GTK_MISC(msg->label), 1.0, 1.0);
	gtk_table_attach(GTK_TABLE(resp->table), msg->label,
	  0, 1, resp->num_components, resp->num_components + 1, 0, 0, 2, 2);
	gtk_table_attach(GTK_TABLE(resp->table), msg->entry,
	  1, 2, resp->num_components, resp->num_components + 1, 0, 0, 2, 2);
	if (!resp->head) resp->head = msg; resp->tail = msg;
	resp->message_list = g_slist_append(resp->message_list, msg);
	break;
      case UH_ERROR_MSG:
	gtk_window_set_title(GTK_WINDOW(resp->top), "Error");
	/* fall through */
      case UH_INFO_MSG:
	msg->label = gtk_label_new(prompt);
	gtk_table_attach(GTK_TABLE(resp->table), msg->label,
	  0, 2, resp->num_components, resp->num_components + 1, 0, 0, 2, 2);
	break;
      case UH_EXPECT_RESP:
	free(msg); /* useless */
	if (--resp->num_components != atoi(prompt)) {
	  /* FIXME: bail out nicely */
          exit (1);
	}
	gtk_widget_show_all(resp->top);
	gtk_widget_grab_focus(resp->head->entry);
	gtk_signal_connect_object(GTK_OBJECT(resp->tail->entry), "activate",
	      gtk_button_clicked, GTK_OBJECT(resp->ok));
	/* FIXME: do all sorts of last-minute stuff */

	resp = NULL; /* start over next time */
	break;
      default:
	/* ignore, I guess... */
	break;
      }
    if (resp) resp->num_components++;
  }

  if(rest != NULL) userhelper_parse_childout(rest);
}

void
userhelper_read_childout(gpointer data, int source, GdkInputCondition cond)
{
  char* output;
  int count;

  if(cond != GDK_INPUT_READ)
    {
      /* serious error, panic. */
      exit (1);
    }

  output = userhelper_malloc(sizeof(char) * MAXLINE);

  count = read(source, output, MAXLINE);
  if (count < 1)
    {
      exit (0);
    }
  output[count] = '\0';

  userhelper_parse_childout(output);
  free(output);
}

void
userhelper_write_childin(GtkWidget *widget, response *resp)
{
  char* input;
  int len;
  message *m;
  GSList *message_list = resp->message_list;

  for (m = message_list->data;
       message_list;
       message_list = message_list->next) {

    input = gtk_entry_get_text(GTK_ENTRY(m->entry));
    len = strlen(input);

    write(childin[1], input, len);
    write(childin[1], "\n", 1);
  }

  gtk_widget_destroy(resp->top);
}

void
userhelper_sigchld()
{
  pid_t pid;
  int status;

  signal(SIGCHLD, userhelper_sigchld);
  
  pid = waitpid(0, &status, WNOHANG);
  
  if(WIFEXITED(status))
    {
      userhelper_parse_exitstatus(WEXITSTATUS(status));
    }
}
