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

#include "userhelper-wrap.h"

#define MAXLINE 512

int childout[2];
int childin[2];


/* the only difference between this and userhelper_run_chfn() is the
 * final exec() call... I want to avoid all duplication, but I can't
 * think of a clean way to do it right now...
 */
void
userhelper_run_passwd()
{
  pid_t pid;

  signal(SIGCHLD, userhelper_sigchld);

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

      gdk_input_add(childout[0], GDK_INPUT_READ, (GdkInputFunction) userhelper_read_childout, NULL);

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

      if(execl(UH_PATH, UH_PATH, UH_PASSWD_OPT, 0) < 0)
	{
	  fprintf(stderr, "execl() error, errno=%d\n", errno);
	}

      _exit(0);

    }
}

void
userhelper_run_chfn(char* fullname, char* office, char* officephone,
		    char* homephone, char* shell)
{
  pid_t pid;

  signal(SIGCHLD, userhelper_sigchld);

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

      gdk_input_add(childout[0], GDK_INPUT_READ, (GdkInputFunction) userhelper_read_childout, NULL);

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

      if(execl(UH_PATH, UH_PATH, 
	       UH_FULLNAME_OPT, fullname,
	       UH_OFFICE_OPT, office, 
	       UH_OFFICEPHONE_OPT, officephone,
	       UH_HOMEPHONE_OPT, homephone,
	       UH_SHELL_OPT, shell,
	       0) < 0)
	{
	  fprintf(stderr, "execl() error, errno=%d\n", errno);
	}

      _exit(0);

    }
}

void
userhelper_parse_exitstatus(int exitstatus)
{
  GtkWidget* message_box;

  switch(exitstatus)
    {
    case 0:
      message_box = create_message_box("Information uptdated.", NULL);
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
      message_box = create_error_box("No such shell in /etc/shells.", NULL);
    case ERR_UNK_ERROR:
      message_box = create_error_box("Unknown error.", NULL);
      break;
    default:
      message_box = create_error_box("SuperUnknown error.", NULL);
      break;
    }

  gtk_signal_connect(GTK_OBJECT(message_box), "destroy", (GtkSignalFunc) userhelper_fatal_error, NULL);
  gtk_widget_show(message_box);

}

void
userhelper_parse_childout(char* outline)
{
  char* prompt;
  int prompt_type;
  GtkWidget* message_box;

  prompt = outline + 2;
  outline[1] = '\0';
  prompt_type = atoi(outline);
  
  switch(prompt_type)
    {
    case UH_ECHO_ON_PROMPT:
      message_box = create_query_box(prompt, NULL, 
				     (GtkSignalFunc)userhelper_write_childin);
      gtk_widget_show(message_box);
      break;
    case UH_ECHO_OFF_PROMPT:
      message_box = create_invisible_query_box(prompt, NULL,
					       (GtkSignalFunc)userhelper_write_childin);
      gtk_widget_show(message_box);
      break;
    case UH_INFO_MSG:
      message_box = create_message_box(prompt, NULL);
      gtk_widget_show(message_box);
      break;
    case UH_ERROR_MSG:
      message_box = create_error_box(prompt, NULL);
      gtk_widget_show(message_box);
      break;
    }
}

void
userhelper_read_childout(gpointer data, int source, GdkInputCondition cond)
{
  char* output;
  int count;

  if(cond != GDK_INPUT_READ)
    {
      /* serious error, panic. */
    }

  output = malloc(sizeof(char) * MAXLINE);

  count = read(source, output, MAXLINE);
  output[count] = '\0';

  userhelper_parse_childout(output);
}

/* void */
/* userhelper_write_childin(gpointer data, int source,
   GdkInputCondition cond) */
void
userhelper_write_childin(GtkWidget* widget, GtkWidget* entry)
{
  /* data should be passed as the entry... */
  char* input;
  int len;

  input = gtk_entry_get_text(GTK_ENTRY(entry));
  len = strlen(input);

/*   write(source, input, len); */
/*   write(source, "\n", 1); */
  write(childin[1], input, len);
  write(childin[1], "\n", 1);
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
