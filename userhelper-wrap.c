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
#include <gdk/gdkx.h>
#include "userhelper-wrap.h"
#include "userdialogs.h"

#define MAXLINE 512

int childout[2];
int childin[2];
int childout_tag;

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
  while((i < 255) && ((args[i++] = va_arg(ap, char *)) != NULL));
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

static void
userhelper_grab_focus(GtkWidget *widget, GdkEvent *map_event, gpointer data)
{
	int ret;
	GtkWidget *toplevel;
	/* grab focus for the toplevel of this widget, so that peers can
	 * get events too */
	toplevel = gtk_widget_get_toplevel(widget);
	ret = gdk_keyboard_grab(toplevel->window, TRUE, GDK_CURRENT_TIME);
}

static void
mark_void(GtkWidget *widget, gpointer target)
{
  if(target != NULL) {
    *(gpointer*)target = NULL;
  }
}

void
userhelper_parse_childout(char* outline)
{
  char *prompt;
  char *rest = NULL;
  char *title;
  int prompt_type;
  static response *resp = NULL;
  GdkPixmap *pixmap;

  if (resp != NULL) {
    if(!GTK_IS_WINDOW(resp->top)) {
      g_free(resp->user);
      g_free(resp);
      resp = NULL;
    }
  }

  if (resp == NULL) {
    GtkWidget *vbox, *hbox, *sbox;

    resp = g_malloc0(sizeof(response));

    resp->user = g_strdup(getlogin());
    resp->top = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_signal_connect(GTK_OBJECT(resp->top), "destroy",
		       GTK_SIGNAL_FUNC(mark_void), &resp->top);
    gtk_window_set_title(GTK_WINDOW(resp->top), "Input");
    gtk_window_position(GTK_WINDOW(resp->top), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(resp->top), 5);
    gtk_signal_connect(GTK_OBJECT(resp->top), "map",
		       GTK_SIGNAL_FUNC(userhelper_grab_focus), NULL);

    resp->ok = gtk_button_new_with_label(UD_OK_TEXT);
    gtk_misc_set_padding(GTK_MISC(GTK_BIN(resp->ok)->child), 4, 0);

    resp->cancel = gtk_button_new_with_label(UD_CANCEL_TEXT);
    gtk_misc_set_padding(GTK_MISC(GTK_BIN(resp->cancel)->child), 4, 0);

    resp->table = gtk_table_new(1, 2, FALSE);
    resp->rows = 1;

    vbox = gtk_vbox_new(FALSE, 4);
    sbox = gtk_hbox_new(TRUE, 4);
    hbox = gtk_hbutton_box_new();
    gtk_box_pack_start(GTK_BOX(vbox), resp->table, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(vbox), sbox, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(sbox), hbox, FALSE, FALSE, 4);
    gtk_container_add(GTK_CONTAINER(resp->top), vbox);

    pixmap = gdk_pixmap_create_from_xpm(gdk_window_foreign_new(GDK_ROOT_WINDOW()),
		                        NULL, NULL, UH_KEY_PIXMAP_PATH);
    if(pixmap != NULL) {
      GtkWidget *pm = gtk_pixmap_new(pixmap, NULL);
      if(pm != NULL) {
	GtkWidget *frame = NULL;
	frame = gtk_frame_new(NULL);
	gtk_container_add(GTK_CONTAINER(frame), pm);
	gtk_table_attach(GTK_TABLE(resp->table), frame,
	  0, 1, 1, 2, GTK_SHRINK, GTK_SHRINK, 2, 2);
	resp->left = 1;
      }
    }

    gtk_box_pack_start(GTK_BOX(hbox), resp->ok, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), resp->cancel, FALSE, FALSE, 0);

    gtk_signal_connect(GTK_OBJECT(resp->top), "delete_event", 
		       (GtkSignalFunc) userhelper_fatal_error, NULL);
    gtk_signal_connect(GTK_OBJECT(resp->cancel), "clicked", 
		       (GtkSignalFunc) userhelper_fatal_error, NULL);

    gtk_signal_connect(GTK_OBJECT(resp->ok), "clicked", 
		       (GtkSignalFunc) userhelper_write_childin, resp);

    gtk_object_set_user_data(GTK_OBJECT(resp->top), resp);
  }

  if(isdigit(outline[0])) {
    gboolean echo = TRUE;
    message *msg = g_malloc(sizeof(message));

    prompt_type = strtol(outline, &prompt, 10);
    if((prompt != NULL) && (strlen(prompt) > 0)) {
      while((isspace(prompt[0]) && (prompt[0] != '\0') && (prompt[0] != '\n'))){
        prompt++;
      }
    }

    /* snip off terminating newlines in the prompt string and save a pointer to
     * interate the parser along */
    rest = strchr(prompt, '\n');
    if(rest != NULL) {
      *rest = '\0';
      rest++;
      if (rest[0] == '\0') {
	rest = NULL;
      }
    }
    /* printf("(%d) \"%s\"\n", prompt_type, prompt); */

    msg->type = prompt_type;
    msg->message = prompt;
    msg->data = NULL;
    msg->entry = NULL;

    echo = TRUE;
    switch(prompt_type) {
      case UH_ECHO_OFF_PROMPT:
	echo = FALSE;
	/* fall through */
      case UH_ECHO_ON_PROMPT:
	msg->label = gtk_label_new(prompt);
	gtk_label_set_line_wrap(GTK_LABEL(msg->label), FALSE);
	gtk_misc_set_alignment(GTK_MISC(msg->label), 1.0, 1.0);

	msg->entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(msg->entry), echo);

	if(resp->head == NULL) resp->head = msg->entry;
	resp->tail = msg->entry;

	gtk_table_attach(GTK_TABLE(resp->table), msg->label,
	  resp->left + 0, resp->left + 1, resp->rows, resp->rows + 1,
	  GTK_EXPAND | GTK_FILL, 0, 2, 2);
	gtk_table_attach(GTK_TABLE(resp->table), msg->entry,
	  resp->left + 1, resp->left + 2, resp->rows, resp->rows + 1,
	  GTK_EXPAND | GTK_FILL, 0, 2, 2);

	resp->message_list = g_slist_append(resp->message_list, msg);
	resp->responses++;
	resp->rows++;
	/* printf("Need %d responses.\n", resp->responses); */
	break;

      case UH_FALLBACK:
#if 0
	msg->label = gtk_label_new(prompt);
	gtk_label_set_line_wrap(GTK_LABEL(msg->label), FALSE);
	gtk_table_attach(GTK_TABLE(resp->table), msg->label,
	  resp->left + 0, resp->left + 2, resp->rows, resp->rows + 1,
	  0, 0, 2, 2);
	resp->message_list = g_slist_append(resp->message_list, msg);
#else
	resp->fallback = atoi(prompt) != 0;
#endif
	break;

      case UH_USER:
	if(strstr(prompt, "<user>") == NULL) {
          g_free(resp->user);
	  resp->user = g_strdup(prompt);
	}
	break;

      case UH_SERVICE_NAME:
	title = g_strdup_printf("Input required to run \"%s\"", prompt);
	gtk_window_set_title(GTK_WINDOW(resp->top), title);
	g_free(title);

	title = NULL;
	if(resp->fallback) {
	  title = g_strdup_printf("In order to run \"%s\" with %s's privileges,"
                                  " additional information is required."
                                  " (It can also be run unprivileged.)",
				  prompt, resp->user);
	} else {
	  title = g_strdup_printf("In order to run \"%s\" with %s's privileges,"
                                  " additional information is required.",
				  prompt, resp->user);
	}
	msg->label = gtk_label_new(title);
	gtk_label_set_line_wrap(GTK_LABEL(msg->label), FALSE);
	gtk_table_attach(GTK_TABLE(resp->table), msg->label,
	  0, resp->left + 2, 0, 1,
	  GTK_EXPAND | GTK_FILL, 0, 2, 2);
	resp->message_list = g_slist_append(resp->message_list, msg);
	break;

      case UH_ERROR_MSG:
	gtk_window_set_title(GTK_WINDOW(resp->top), "Error");
	/* fall through */
      case UH_INFO_MSG:
	msg->label = gtk_label_new(prompt);
	gtk_label_set_line_wrap(GTK_LABEL(msg->label), FALSE);
	gtk_table_attach(GTK_TABLE(resp->table), msg->label,
	  resp->left + 0, resp->left + 2, resp->rows, resp->rows + 1, 0, 0, 2, 2);
	resp->message_list = g_slist_append(resp->message_list, msg);
	resp->rows++;
	break;

      case UH_EXPECT_RESP:
	g_free(msg); /* useless */
	if (resp->responses != atoi(prompt)) {
          printf("You want %d response(s) from %d entry fields!?!?!\n",
                 atoi(prompt), resp->responses);
          exit (1);
	}
	gtk_widget_show_all(resp->top);
	if(GTK_IS_ENTRY(resp->head)) {
	  gtk_widget_grab_focus(resp->head);
	}
	if(GTK_IS_ENTRY(resp->tail)) {
	  gtk_signal_connect_object(GTK_OBJECT(resp->tail), "activate",
	    gtk_button_clicked, GTK_OBJECT(resp->ok));
	}
	break;
      default:
	/* ignore, I guess... */
	break;
      }
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
      /* Serious error, this is.  Panic. */
      exit (1);
    }

  output = g_malloc(MAXLINE + 1);

  count = read(source, output, MAXLINE);
  if (count == -1)
    {
      exit (0);
    }
  output[count] = '\0';

  userhelper_parse_childout(output);
  g_free(output);
}

void
userhelper_write_childin(GtkWidget *widget, response *resp)
{
  char* input;
  int len;
  GSList *message_list = resp->message_list;

  for (message_list = resp->message_list;
       (message_list != NULL) && (message_list->data != NULL);
       message_list = g_slist_next(message_list)) {
    if(GTK_IS_ENTRY(((message*)message_list->data)->entry)) {
      message *m = (message*)message_list->data;
      input = gtk_entry_get_text(GTK_ENTRY(m->entry));
      len = strlen(input);
      write(childin[1], input, len);
      write(childin[1], "\n", 1);
    }
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
