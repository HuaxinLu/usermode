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

/* Things to remeber...
 * -- when setting values, make sure there are no colons in what users
 * want me to set.  This is just for convenient dialog boxes to tell
 * people to remove their colons.  Presumably, the suid root helper to
 * actually set values won't accept anything that has colons.  Ditto
 * for commas, as well... but the suid helper doesn't need to deal
 * with that.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <gtk/gtk.h>

#define NUM_GECOS_FIELDS 4
#define MAXLINE 512

struct _UserInfo
{
  /* use GString* or gchar*? */
  char* full_name;
  char* office;
  char* office_phone;
  char* home_phone;
  char* shell;
};

typedef struct _UserInfo UserInfo;

void create_userinfo_window(UserInfo* userinfo);
GtkWidget* create_shell_menu(UserInfo* userinfo);
GtkWidget* create_gecos_table(UserInfo* userinfo);
GtkWidget* create_help_dialog();
GtkWidget* create_message_box(char* message);
GtkWidget* create_error_box(char* error);
GtkWidget* create_query_box(char* prompt);
GtkWidget* create_invisible_query_box(char* prompt);
void ok_button(UserInfo* userinfo);
void show_help_dialog();
UserInfo* parse_userinfo();
void set_new_userinfo();

int
main(int argc, char* argv[])
{
  UserInfo* userinfo;

  /* gtk_set_locale(); */		/* this is new... */
  gtk_init(&argc, &argv);
  /* put this back in when I've decided I need it... */
  /*   gtk_rc_parse("userinforc"); */

  userinfo = parse_userinfo();

  create_userinfo_window(userinfo);

  gtk_main();

  return 0;

}

void
create_userinfo_window(UserInfo* userinfo)
{
  GtkWidget* main;		/* GtkWindow */
  GtkWidget* gecos;		/* GtkTable */
  GtkWidget* shell_field;	/* GtkOptionMenu */
  GtkWidget* buttons;		/* GtkHBox */
  GtkWidget* ok;		/* GtkButton */
  GtkWidget* cancel;		/* GtkButton */
  GtkWidget* help;		/* GtkButton */
  
  /* create the widgets */
  /*   main = gtk_window_new(GTK_WINDOW_TOPLEVEL); */
  main = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(main), "User Information");
  /*   gtk_container_border_width(GTK_CONTAINER(main), 5); */
  gtk_signal_connect(GTK_OBJECT(main), "destroy",
		     (GtkSignalFunc) gtk_exit, NULL);

  gecos = create_gecos_table(userinfo);

  shell_field = create_shell_menu(userinfo);

  gtk_table_attach(GTK_TABLE(gecos), shell_field,
		   1, 2, 4, 5,
		   0, 0, 0, 0);
  gtk_widget_show(shell_field);

  /* the buttons at the bottom */
  buttons = gtk_hbox_new(FALSE, 0);
  ok = gtk_button_new_with_label("OK");
  gtk_signal_connect(GTK_OBJECT(ok), "clicked", 
		     (GtkSignalFunc) ok_button, userinfo);
  gtk_widget_show(ok);
  cancel = gtk_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT(cancel), "clicked", 
		     (GtkSignalFunc) gtk_exit, NULL);
  gtk_widget_show(cancel);
  help = gtk_button_new_with_label("Help");
  gtk_signal_connect(GTK_OBJECT(help), "clicked",
		     (GtkSignalFunc) show_help_dialog, NULL);
  gtk_widget_show(help);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     ok, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     cancel, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     help, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->vbox), gecos, TRUE, TRUE, 0);

  gtk_widget_show(gecos);
  gtk_widget_show(buttons);
  gtk_widget_show(main);
}

GtkWidget*
create_shell_menu(UserInfo* userinfo)
{
  GtkWidget* shell_menu;
  GtkWidget* menu;
  GtkWidget* menuitem;
  int shells_fd;
  struct stat shells_stat;
  char* shells_buf;
  char* shell_curr;
  char* shell_sep = "\n";

  shell_menu = gtk_option_menu_new();
  menu = gtk_menu_new();

  menuitem = gtk_menu_item_new_with_label(userinfo->shell);
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  /* FIXME: aggressive error checking... */
  shells_fd = open("/etc/shells", O_RDONLY);
  fstat(shells_fd, &shells_stat);
  shells_buf = malloc(sizeof(char) * shells_stat.st_size);
  read(shells_fd, shells_buf, shells_stat.st_size);

  shell_curr = strtok(shells_buf, shell_sep);
  if(strcmp(shell_curr, userinfo->shell) != 0)
    {
      menuitem = gtk_menu_item_new_with_label(shell_curr);
      gtk_menu_append(GTK_MENU(menu), menuitem);
      gtk_widget_show(menuitem);
    }

  shell_curr = strtok(NULL, shell_sep);
  if(strcmp(shell_curr, userinfo->shell) != 0)
    {
      menuitem = gtk_menu_item_new_with_label(shell_curr);
      gtk_menu_append(GTK_MENU(menu), menuitem);
      gtk_widget_show(menuitem);
    }

  shell_curr = strtok(NULL, shell_sep);
  if(strcmp(shell_curr, userinfo->shell) != 0)
    {
      menuitem = gtk_menu_item_new_with_label(shell_curr);
      gtk_menu_append(GTK_MENU(menu), menuitem);
      gtk_widget_show(menuitem);
    }

  shell_curr = strtok(NULL, shell_sep);
  if(strcmp(shell_curr, userinfo->shell) != 0)
    {
      menuitem = gtk_menu_item_new_with_label(shell_curr);
      gtk_menu_append(GTK_MENU(menu), menuitem);
      gtk_widget_show(menuitem);
    }

  shell_curr = strtok(NULL, shell_sep);
  if(strcmp(shell_curr, userinfo->shell) != 0)
    {
      menuitem = gtk_menu_item_new_with_label(shell_curr);
      gtk_menu_append(GTK_MENU(menu), menuitem);
      gtk_widget_show(menuitem);
    }

  shell_curr = strtok(NULL, shell_sep);
  if(strcmp(shell_curr, userinfo->shell) != 0)
    {
      menuitem = gtk_menu_item_new_with_label(shell_curr);
      gtk_menu_append(GTK_MENU(menu), menuitem);
      gtk_widget_show(menuitem);
    }

  shell_curr = strtok(NULL, shell_sep);
  if(strcmp(shell_curr, userinfo->shell) != 0)
    {
      menuitem = gtk_menu_item_new_with_label(shell_curr);
      gtk_menu_append(GTK_MENU(menu), menuitem);
      gtk_widget_show(menuitem);
    }

  gtk_option_menu_set_menu(GTK_OPTION_MENU(shell_menu), menu);
  return shell_menu;
}

GtkWidget*
create_gecos_table(UserInfo* userinfo)
{
  GtkWidget* gecos;		/* GtkTable */
  GtkWidget* gecos_fields[NUM_GECOS_FIELDS];
  GtkWidget* label;
  struct passwd* pwent;
  char* gecos_raw;
  char* gecos_sep = ",";
  int gecos_name_only = FALSE;
  int i;

  pwent = getpwuid(getuid());

  gecos_raw = strdup(pwent->pw_gecos);

  if(strchr(gecos_raw, gecos_sep[0]) == NULL)
    {
      gecos_name_only = TRUE;
    }
    
  gecos = gtk_table_new(NUM_GECOS_FIELDS + 1, 2, FALSE);
  gtk_container_border_width(GTK_CONTAINER(gecos), 5);

  gtk_table_set_row_spacings(GTK_TABLE(gecos), 5);
  gtk_table_set_col_spacings(GTK_TABLE(gecos), 5);

  i = 0;

  label = gtk_label_new("Full Name:");
  gtk_table_attach(GTK_TABLE(gecos), label, 
		   0, 1, i, i+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(label);
  gecos_fields[i] = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(gecos_fields[i]), strtok(gecos_raw, gecos_sep));
  gtk_widget_show(gecos_fields[i]);
  gtk_table_attach(GTK_TABLE(gecos), gecos_fields[i], 
		   1, 2, i, i+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  i++;

  label = gtk_label_new("Office:");
  gtk_table_attach(GTK_TABLE(gecos), label, 
		   0, 1, i, i+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(label);

  gecos_fields[i] = gtk_entry_new();
  gtk_widget_show(gecos_fields[i]);
  gtk_table_attach(GTK_TABLE(gecos), gecos_fields[i], 
		   1, 2, i, i+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  if(!gecos_name_only)
    {
      gtk_entry_set_text(GTK_ENTRY(gecos_fields[i]), strtok(NULL, gecos_sep));
    }
  i++;

  label = gtk_label_new("Office Phone:");
  gtk_table_attach(GTK_TABLE(gecos), label, 
		   0, 1, i, i+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(label);
  gecos_fields[i] = gtk_entry_new();
  gtk_widget_show(gecos_fields[i]);
  gtk_table_attach(GTK_TABLE(gecos), gecos_fields[i], 
		   1, 2, i, i+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  if(!gecos_name_only)
    {
      gtk_entry_set_text(GTK_ENTRY(gecos_fields[i]), strtok(NULL, gecos_sep));
    }
  i++;

  label = gtk_label_new("Home Phone:");
  gtk_table_attach(GTK_TABLE(gecos), label, 
		   0, 1, i, i+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(label);
  gecos_fields[i] = gtk_entry_new();
  gtk_widget_show(gecos_fields[i]);
  gtk_table_attach(GTK_TABLE(gecos), gecos_fields[i], 
		   1, 2, i, i+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  if(!gecos_name_only)
    {
      gtk_entry_set_text(GTK_ENTRY(gecos_fields[i]), strtok(NULL, gecos_sep));
    }
  i++;

  label = gtk_label_new("Shell:");
  gtk_table_attach(GTK_TABLE(gecos), label,
		   0, 1, i, i+1,
		   GTK_EXPAND | GTK_FILL,
		   GTK_EXPAND | GTK_FILL,
		   0, 0);
  gtk_widget_show(label);
  /* the shell widget is added elsewhere */

  return gecos;
}

UserInfo* 
parse_userinfo()
{
  /* do serious error checking */

  UserInfo* retval;
  struct passwd* pwent;
  char* gecos;
  char* pwsep = ":";

  retval = malloc(sizeof(UserInfo));

  pwent = getpwuid(getuid());

  retval->shell = strdup(pwent->pw_shell);
  gecos = strdup(pwent->pw_gecos);

  if(strchr(gecos, pwsep[0]) == NULL)
    {
      retval->full_name = gecos;
      /* think this is valid, right? */
      retval->office = "";
      retval->office_phone = "";
      retval->home_phone = "";
    }
  else
    {
      /* if, for some reason this gets threaded, wrap this in #ifdef
       * _REENTRANT with using strtok_r, for thread saftey.
       */
      retval->full_name = strtok(gecos, pwsep);
      retval->office = strtok(NULL, pwsep);
      retval->office_phone = strtok(NULL, pwsep);
      retval->home_phone = strtok(NULL, pwsep);
    }

  return retval;
}

GtkWidget*
create_help_dialog()
{
  static GtkWidget* help_dialog;
  GtkWidget* label;
  GtkWidget* ok;

  if(help_dialog != NULL)
    {
      return help_dialog;
    }

  help_dialog = gtk_dialog_new();
  label = gtk_label_new("This will be some help text.");
  ok = gtk_button_new_with_label("OK");
  gtk_signal_connect(GTK_OBJECT(ok), "clicked", 
		     (GtkSignalFunc) show_help_dialog, NULL);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(help_dialog)->vbox), label, 
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(help_dialog)->action_area),
		     ok, FALSE, FALSE, 0); 

  gtk_widget_show(label);
  gtk_widget_show(ok);
  

  return help_dialog;
}

void
ok_button(UserInfo* userinfo)
{
  set_new_userinfo();
}

void
show_help_dialog()
{
  GtkWidget* help_dialog;
  static int status = FALSE;

  help_dialog = create_help_dialog();

  if(!status)
    {
      gtk_widget_show(help_dialog);
      status = !status;
    }
  else
    {
      gtk_widget_hide(help_dialog);
      status = !status;
    }

}

void
set_new_userinfo()
{
  /* get the changed information... blech... how do I manage that */
  /* fork() and exec() userhelper with the appropriate args.... */

/*   char* userhelper = "/usr/sbin/userhelper"; */
  char* userhelper = "./userhelper";
  char* fullname_opt = "-f";
  char* office_opt = "-o";
  char* officephone_opt = "-p";
  char* homephone_opt = "-h";

  /* the new values... */
  char* fullname;
  char* office;
  char* officephone;
  char* homephone;

  int childout[2];
  /* supposedly the helper doesn't put anything on stderr... */
/*   int childerr[2]; */
  pid_t pid;

  char outline[MAXLINE];
/*   char errline[MAXLINE]; */
  int n;
  int count;
  int childstatus;

  /* stuff for the select */
  

  /* need to figure out the new values.. how the hell am I going to
   * manage that? 
   */

/*   if(pipe(childout) < 0 || pipe(childerr) < 0) */
  if(pipe(childout) < 0)
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
/*       close(childerr[1]); */

      n = 0;

/*       count = read(childerr[0], errline, MAXLINE); */
/*       if(count > 0) */
/* 	{ */
/* 	  errline[count] = '\0'; */
/* 	  message_box = create_message_box(errline); */
/* 	  gtk_widget_show(message_box); */
/* 	} */
      count = read(childout[0], outline, MAXLINE);
      if(count > 0)
	{
	  outline[count] = '\0';
	  message_box = create_message_box(outline);
	  gtk_widget_show(message_box);
	}
      return FALSE;

    }
  else				/* child */
    {
      close(childout[0]);
/*       close(childerr[0]); */

      if(childout[1] != STDOUT_FILENO)
	{
	  if(dup2(childout[1], STDOUT_FILENO) != STDOUT_FILENO)
	    {
	      fprintf(stdout, "dup2() error.\n");
	      exit(2);
	    }
	}
/*       if(childerr[1] != STDERR_FILENO) */
/* 	{ */
/* 	  if(dup2(childerr[1], STDERR_FILENO) != STDERR_FILENO) */
/* 	    { */
/* 	      fprintf(stdout, "dup2() error.\n"); */
/* 	      exit(2); */
/* 	    } */
/* 	} */

      if(execl(userhelper, userhelper, 
	       fullname_opt, fullname,
	       office_opt, office, 
	       officephone_opt, officephone_opt, 
	       homephone_opt, homephone, 0) < 0)
	{
	  fprintf(stderr, "execl() error, errno=%d\n", errno);
	}

      _exit(0);

    }

}

GtkWidget*
create_message_box(char* message)
{

}

GtkWidget*
create_error_box(char* error)
{

}

GtkWidget*
create_query_box(char* prompt)
{

}

GtkWidget*
create_invisible_query_box(char* prompt)
{

}
