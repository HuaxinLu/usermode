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
  GtkWidget* message_box;
  char* prompt;
  int prompt_type;

  pid_t pid;
  int childstatus;
  char outline[MAXLINE];
  int count;

  fd_set rfds;
  fd_set wfds;
  struct timeval tv;
  int retval;

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

      FD_ZERO(&rfds);
      FD_SET(childout[0], &rfds);
      FD_ZERO(&wfds);
      FD_SET(childin[1], &wfds);

      /* switch to gdk_input() */
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

}

void
userhelper_parse_exitstatus(int exitstatus)
{

}

void
userhelper_parse_childout()
{

}

int
userhelper_read_childout()
{

}

void
userhelper_write_childin(GtkWidget* widget, GtkWidget* entry)
{
  char* input;
  int len;

  input = gtk_entry_get_text(GTK_ENTRY(entry));

  len = strlen(input);
  write(childin[1], input, len);
  write(childin[1], "\n", 1);

}

void
userhelper_sigchld()
{
  /* if the child died, get it's exitstatus and parse it. */
  /* don't forget to reset the signal handler... */
}
