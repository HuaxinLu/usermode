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

/* I may not need quite all of these, but good enough for now... */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <gtk/gtk.h>

#define MAXLINE 512

/* fix userhelper, so these match the pam.h defines... */
#define ECHO_ON_PROMPT 1
#define ECHO_OFF_PROMPT 2
#define INFO_PROMPT 3
#define ERROR_PROMPT 4

void userpasswd();
GtkWidget* create_message_box(char* message);
GtkWidget* create_error_box(char* error);
GtkWidget* create_query_box(char* prompt, int fd);
GtkWidget* create_invisible_query_box(char* prompt, int fd);
void user_input(GtkWidget* widget, int fd);

int
main(int argc, char* argv[])
{
  gtk_init(&argc, &argv);

  userpasswd();

  gtk_main();

  return 0;
}

void
userpasswd()
{
  GtkWidget* message_box;

  char* userhelper = "/usr/sbin/userhelper";
  char* passwd_opt = "-c";
  char* prompt;
  int prompt_type;

  int childout[2];
  int childin[2];
  pid_t pid;
  int childstatus;
  char outline[MAXLINE];
  int count;

  fd_set rfds;
  fd_set wfds;
  struct timeval tv;
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

      FD_ZERO(&rfds);
      FD_SET(childout[0], &rfds);
      FD_ZERO(&wfds);
      FD_SET(childin[1], &wfds);

      retval = select(childout[0] + 1, &rfds, &wfds, NULL, NULL);
      while(retval > 0)
      {
	if(FD_ISSET(childout[0], &rfds))
	  {
	    /* technically, I should keep reading until everything is
	     * exhausted... in practice, there isn't enough data to
	     * exceed MAXLINE.
	     */
	    count = read(childout[0], outline, MAXLINE);
	    outline[count] = '\0';

	    prompt = outline + 2;
	    outline[1] = '\0';
	    prompt_type = atoi(outline);

	    switch(prompt_type)
	      {
	      case ECHO_ON_PROMPT:
		message_box = create_query_box(prompt, childin[1]);
		gtk_widget_show(message_box);
		break;
	      case ECHO_OFF_PROMPT:
		message_box = create_invisible_query_box(prompt, childin[1]);
		gtk_widget_show(message_box);
		break;
	      case INFO_PROMPT:
		message_box = create_message_box(prompt);
		gtk_widget_show(message_box);
		break;
	      case ERROR_PROMPT:
		message_box = create_error_box(prompt);
		gtk_widget_show(message_box);
		break;
	      }
	    
	  }

	waitpid(pid, &childstatus, WNOHANG);

	if(WIFEXITED(childstatus) != 0)
	  {
 	    retval = 0;
	  }
	else
	  {
	    tv.tv_sec = 0;
 	    tv.tv_usec = 50;	/* long enough? */
	    retval = select(childout[0] + 1, &rfds, &wfds, NULL, &tv);
	  }
      }
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

      if(execl(userhelper, userhelper, passwd_opt, 0) < 0)
	{
	  fprintf(stderr, "execl() error, errno=%d\n", errno);
	}

      _exit(0);

    }

}

GtkWidget*
create_message_box(char* message)
{
  /* need to put this and other functions into a seperate file, so
   * both usermount and userinfo can use 'em... and probably
   * userpasswd as well.
   */
  GtkWidget* message_box;
  GtkWidget* label;
  GtkWidget* ok;

  message_box = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(message_box), "Message");

  label = gtk_label_new(message);
  ok = gtk_button_new_with_label("OK");
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", 
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) message_box);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(message_box)->vbox), label,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(message_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(message_box);

  return message_box;

}

GtkWidget*
create_error_box(char* error)
{
  GtkWidget* error_box;
  GtkWidget* label;
  GtkWidget* ok;

  error_box = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(error_box), "Error");

  label = gtk_label_new(error);
  ok = gtk_button_new_with_label("OK");
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", 
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) error_box);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(error_box)->vbox), label,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(error_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(error_box);

  return error_box;
}

GtkWidget*
create_query_box(char* prompt, int fd)
{
  GtkWidget* query_box;
  GtkWidget* label;
  GtkWidget* entry;
  GtkWidget* ok;

  query_box = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(query_box), "Prompt");

  label = gtk_label_new(prompt);
  entry = gtk_entry_new();
  ok = gtk_button_new_with_label("OK");
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", 
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) query_box);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), label,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), entry,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(entry);
  gtk_widget_show(query_box);

  return query_box;
}

GtkWidget*
create_invisible_query_box(char* prompt, int fd)
{
  /* Making the text invisible by setting the bg and fg color the
   * same... there may be an exploit by putting something in gtkrc
   * that sets the style to something else and makes the text
   * visible.  I'm not sure, though.. I'll have to see if I can do
   * it... if so, I'm sure there's some workaround. (Worst case, don't
   * parse any rc files, but that's kinda extreme and I do want other
   * settings like users fonts and such.. )
   */
  GtkWidget* query_box;
  GtkWidget* label;
  GtkWidget* entry;
  GtkWidget* ok;
  
  query_box = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(query_box), "Prompt");

  label = gtk_label_new(prompt);
  entry = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
  gtk_signal_connect(GTK_OBJECT(entry), "destroy",
		     (GtkSignalFunc) user_input, (gpointer)fd);
  ok = gtk_button_new_with_label("OK");
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked",
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) query_box);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), label,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), entry,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(entry);
  gtk_widget_show(query_box);

  return query_box;
}

/* function to take user's input and write it to a file descriptor */
void
user_input(GtkWidget* widget, int fd)
{
  /* FIXME: aggressive error checking */
  char* input;
  int len;

  input = gtk_entry_get_text(GTK_ENTRY(widget));

  len = strlen(input);
  write(fd, input, len);
  write(fd, "\n", 1);

}
