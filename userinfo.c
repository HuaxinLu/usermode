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

/* Things to remember...
 * -- when setting values, make sure there are no colons in what users
 * want me to set.  This is just for convenient dialog boxes to tell
 * people to remove their colons.  Presumably, the suid root helper to
 * actually set values won't accept anything that has colons.  Ditto
 * for commas, as well... but the suid helper doesn't need to deal
 * with that.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <gtk/gtk.h>
#include "userdialogs.h"
#include "userhelper-wrap.h"

#define NUM_GECOS_FIELDS 5
/* #define MAXLINE 512 */

/* fix userhelper, so these match the pam.h defines... */
/* #define ECHO_ON_PROMPT 1 */
/* #define ECHO_OFF_PROMPT 2 */
/* #define INFO_PROMPT 3 */
/* #define ERROR_PROMPT 4 */

#define GECOS_FULLNAME 0
#define GECOS_OFFICE 1
#define GECOS_OFFICEPHONE 2
#define GECOS_HOMEPHONE 3
#define GECOS_SHELL 4

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

/* global, yich... should go into the UserInfo struct... */
GtkWidget* gecos_fields[NUM_GECOS_FIELDS];

void create_userinfo_window(UserInfo* userinfo);
GtkWidget* create_shell_menu(UserInfo* userinfo);
GtkWidget* create_gecos_table(UserInfo* userinfo);
GtkWidget* create_help_dialog();

void ok_button(GtkWidget* widget, UserInfo* userinfo);
void shell_select();
/* void user_input(GtkWidget* widget, GtkWidget* entry); */
void show_help_dialog();
UserInfo* parse_userinfo();
void set_new_userinfo(UserInfo* userinfo);

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
  GtkWidget* mainwindow;		/* GtkWindow */
  GtkWidget* gecos;		/* GtkTable */
  GtkWidget* shell_field;	/* GtkOptionMenu */
  GtkWidget* ok;		/* GtkButton */
  GtkWidget* cancel;		/* GtkButton */
  GtkWidget* help;		/* GtkButton */
  
  /* create the widgets */
  mainwindow = gtk_dialog_new();
  gtk_container_set_border_width(GTK_CONTAINER(mainwindow), 5);
  gtk_window_set_title(GTK_WINDOW(mainwindow), "User Information");
  gtk_signal_connect(GTK_OBJECT(mainwindow), "destroy",
		     (GtkSignalFunc) gtk_main_quit, NULL);
  gtk_signal_connect(GTK_OBJECT(mainwindow), "delete_event",
		     (GtkSignalFunc) gtk_main_quit, NULL);

  gecos = create_gecos_table(userinfo);

  shell_field = create_shell_menu(userinfo);

  gtk_table_attach(GTK_TABLE(gecos), shell_field,
		   1, 2, 4, 5,
		   0, 0, 0, 0);
  gtk_widget_show(shell_field);

  ok = gtk_button_new_with_label("OK");
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(ok)->child), 4, 0);

/*   GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT); */
/*   gtk_widget_grab_default(ok); */
  gtk_signal_connect(GTK_OBJECT(ok), "clicked", 
		     (GtkSignalFunc) ok_button, userinfo);
  gtk_widget_show(ok);

  cancel = gtk_button_new_with_label(UD_EXIT_TEXT);
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(cancel)->child), 4, 0);
  gtk_signal_connect(GTK_OBJECT(cancel), "clicked", 
		     (GtkSignalFunc) gtk_main_quit, NULL);
  gtk_widget_show(cancel);
  help = gtk_button_new_with_label("Help");
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(help)->child), 4, 0);
  gtk_signal_connect(GTK_OBJECT(help), "clicked",
		     (GtkSignalFunc) show_help_dialog, NULL);
/*   gtk_widget_show(help); */

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mainwindow)->action_area), 
		     ok, TRUE, TRUE, 2);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mainwindow)->action_area), 
		     cancel, TRUE, TRUE, 2);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mainwindow)->action_area), 
		     help, TRUE, TRUE, 2);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(mainwindow)->vbox), gecos, TRUE, TRUE, 0);

  gtk_widget_show(gecos);
  gtk_widget_show(mainwindow);
}

GtkWidget*
create_shell_menu(UserInfo* userinfo)
{
  GtkWidget* shell_menu;
  GtkWidget* menu;
  GtkWidget* menuitem;
  char* shell_curr;

  shell_menu = gtk_option_menu_new();
  menu = gtk_menu_new();
  gtk_widget_set_sensitive(shell_menu, FALSE);

  /* shell isn't technically a gecos field, so I should probably
   * change the name of these symbols, but oh well for now... this
   * makes my life easy elsehwere.
   * The gtk_entry allocated here isn't ever actually shown, it's just
   * a place to stick a string.
   */
  gecos_fields[GECOS_SHELL] = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(gecos_fields[GECOS_SHELL]), userinfo->shell);

  menuitem = gtk_menu_item_new_with_label(userinfo->shell);
  gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
		     (GtkSignalFunc) shell_select, userinfo->shell);
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  setusershell();
  for(shell_curr = getusershell(); shell_curr; shell_curr = getusershell())
    {
      if(strcmp(shell_curr, userinfo->shell) != 0)
	{
	  menuitem = gtk_menu_item_new_with_label(shell_curr);
	  gtk_signal_connect(GTK_OBJECT(menuitem), "activate", 
			     (GtkSignalFunc) shell_select, shell_curr);
	  gtk_menu_append(GTK_MENU(menu), menuitem);
	  gtk_widget_show(menuitem);
	}
      else
	{
	  /* The user's shell is a valid one.  Turn the option menu on. */
	  gtk_widget_set_sensitive(shell_menu, TRUE);
	}
      shell_curr = getusershell();
    }
  endusershell();

  gtk_option_menu_set_menu(GTK_OPTION_MENU(shell_menu), menu);
  return shell_menu;
}

GtkWidget*
create_gecos_table(UserInfo* userinfo)
{
  GtkWidget* gecos;		/* GtkTable */
/*   GtkWidget* gecos_fields[NUM_GECOS_FIELDS]; */
  GtkWidget* label;
  struct passwd* pwent;
  int i;

  pwent = getpwuid(getuid());

  gecos = gtk_table_new(NUM_GECOS_FIELDS + 1, 2, FALSE);
  gtk_container_border_width(GTK_CONTAINER(gecos), 5);

  gtk_table_set_row_spacings(GTK_TABLE(gecos), 5);
  gtk_table_set_col_spacings(GTK_TABLE(gecos), 5);

  i = GECOS_FULLNAME;

  label = gtk_label_new("Full Name:");
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
  gtk_entry_set_text(GTK_ENTRY(gecos_fields[i]), userinfo->full_name);

  i = GECOS_OFFICE;

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
  gtk_entry_set_text(GTK_ENTRY(gecos_fields[i]), userinfo->office);

  i = GECOS_OFFICEPHONE;

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
  gtk_entry_set_text(GTK_ENTRY(gecos_fields[i]), userinfo->office_phone);

  i = GECOS_HOMEPHONE;

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
  gtk_entry_set_text(GTK_ENTRY(gecos_fields[i]), userinfo->home_phone);

  i = GECOS_SHELL;

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
  UserInfo* retval;
  struct passwd* pwent;

  char* gecos_raw;
  char* gecos_tok;

  retval = calloc(sizeof(UserInfo), 1);

  pwent = getpwuid(getuid());

  retval->shell = strdup(pwent->pw_shell);
  gecos_raw = strdup(pwent->pw_gecos);
  gecos_tok = gecos_raw;
  retval->office = "";
  retval->office_phone = "";
  retval->home_phone = "";
  
  retval->full_name = gecos_raw;

  while (*gecos_tok && (*gecos_tok != ',')) gecos_tok++;
  if (*gecos_tok) *(gecos_tok++) = '\0';
  if (*gecos_tok && (*gecos_tok != ',')) retval->office = gecos_tok;

  while (*gecos_tok && (*gecos_tok != ',')) gecos_tok++;
  if (*gecos_tok) *(gecos_tok++) = '\0';
  if (*gecos_tok && (*gecos_tok != ',')) retval->office_phone = gecos_tok;

  while (*gecos_tok && (*gecos_tok != ',')) gecos_tok++;
  if (*gecos_tok) *(gecos_tok++) = '\0';
  if (*gecos_tok && (*gecos_tok != ',')) retval->home_phone = gecos_tok;

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
  gtk_container_set_border_width(GTK_CONTAINER(help_dialog), 5);
  label = gtk_label_new("This will be some help text.");
  ok = gtk_button_new_with_label("OK");
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(ok)->child), 4, 0);
  gtk_signal_connect(GTK_OBJECT(ok), "clicked", 
		     (GtkSignalFunc) show_help_dialog, NULL);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(help_dialog)->vbox), label, 
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(help_dialog)->action_area),
		     ok, FALSE, FALSE, 0); 

  GTK_WIDGET_SET_FLAGS(ok, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(ok);

  gtk_widget_show(label);
  gtk_widget_show(ok);
  

  return help_dialog;
}

void
ok_button(GtkWidget* widget, UserInfo* userinfo)
{
  set_new_userinfo(userinfo);
/*   gtk_exit(0); */
}

void
shell_select(GtkWidget* widget, char* shell)
{
  gtk_entry_set_text(GTK_ENTRY(gecos_fields[GECOS_SHELL]), shell);
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
set_new_userinfo(UserInfo* userinfo)
{
  char* fullname;
  char* office;
  char* officephone;
  char* homephone;
  char* shell;
  char *argv[12];
  int i = 0;

  fullname = gtk_entry_get_text(GTK_ENTRY(gecos_fields[GECOS_FULLNAME]));
  office = gtk_entry_get_text(GTK_ENTRY(gecos_fields[GECOS_OFFICE]));
  officephone = gtk_entry_get_text(GTK_ENTRY(gecos_fields[GECOS_OFFICEPHONE]));
  homephone = gtk_entry_get_text(GTK_ENTRY(gecos_fields[GECOS_HOMEPHONE]));
  shell = gtk_entry_get_text(GTK_ENTRY(gecos_fields[GECOS_SHELL]));


  argv[i++] = UH_PATH;

  argv[i++] = UH_FULLNAME_OPT;
  if (fullname && fullname[0])
    argv[i++] = fullname;
  else
    argv[i++] = "";

  argv[i++] = UH_OFFICE_OPT;
  if (office && office[0])
    argv[i++] = office;
  else
    argv[i++] = "";

  argv[i++] = UH_OFFICEPHONE_OPT;
  if (officephone && officephone[0])
    argv[i++] = officephone;
  else
    argv[i++] = "";

  argv[i++] = UH_HOMEPHONE_OPT;
  if (homephone && homephone[0])
    argv[i++] = homephone;
  else
    argv[i++] = "";

  argv[i++] = UH_SHELL_OPT;
  if (shell && shell[0])
    argv[i++] = shell;
  else
    argv[i++] = "";

  argv[i++] = NULL;

  signal(SIGCHLD, userhelper_sigchld);
  userhelper_runv(UH_PATH, argv);
}

void
userhelper_fatal_error(int ignored)
{
  gtk_main_quit();
}
