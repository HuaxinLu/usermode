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
GtkWidget* create_gecos_table(UserInfo* userinfo, int rows, int cols);
UserInfo* parse_userinfo();

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
  GtkWidget* notebook;		/* GtkNotebook */
  GtkWidget* gecos;		/* GtkTable */
  GtkWidget* shell_field;	/* GtkOptionMenu */
  GtkWidget* password;		/* GtkVBox */
  GtkWidget* pw_info;		/* GtkLabel */
  GtkWidget* pw_field_old;	/* GtkEntry */
  GtkWidget* pw_field_1;	/* GtkEntry */ /* need gtk_entry_set_echo_char() */
  GtkWidget* pw_field_2;	/* GtkEntry */ /* ditto */
  GtkWidget* buttons;		/* GtkHBox */
  GtkWidget* ok;		/* GtkButton */
  GtkWidget* cancel;		/* GtkButton */
  GtkWidget* help;		/* GtkButton */
  
  /* create the widgets */
  /*   main = gtk_window_new(GTK_WINDOW_TOPLEVEL); */
  main = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(main), "User Info");
  /*   gtk_container_border_width(GTK_CONTAINER(main), 5); */
  gtk_signal_connect(GTK_OBJECT(main), "destroy",
		     (GtkSignalFunc) gtk_exit, NULL);

  notebook = gtk_notebook_new();
  gtk_container_border_width(GTK_CONTAINER(notebook), 5);

  gecos = create_gecos_table(userinfo, NUM_GECOS_FIELDS, 2);

  shell_field = create_shell_menu(userinfo);

  gtk_table_attach(GTK_TABLE(gecos), shell_field,
		   1, 2, 4, 5,
		   0, 0, 0, 0);
  gtk_widget_show(shell_field);

  /* build password widgets */
  password = gtk_vbox_new(FALSE, 0);

  pw_field_old = gtk_entry_new();
  pw_field_1 = gtk_entry_new();
  pw_field_2 = gtk_entry_new();
  gtk_widget_set_sensitive(pw_field_1, FALSE);
  gtk_widget_set_sensitive(pw_field_2, FALSE);
  /* need to when enter is hit on pw_field_old, turn the others back
   * on... enter on 1 moves focus to 2, enter on 2 checks equality, if
   * they're equal, then it sets the password.
   */
  gtk_box_pack_start(GTK_BOX(password), pw_field_old, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(password), pw_field_1, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(password), pw_field_2, FALSE, FALSE, 0);

  /* the buttons at the bottom */
  buttons = gtk_hbox_new(FALSE, 0);
  ok = gtk_button_new_with_label("OK");
  gtk_widget_show(ok);
  cancel = gtk_button_new_with_label("Cancel");
  gtk_widget_show(cancel);
  help = gtk_button_new_with_label("Help");
  gtk_widget_show(help);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     ok, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     cancel, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     help, FALSE, FALSE, 0);

  /* build the notebook... */
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), gecos, gtk_label_new("General"));
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), password,
			   gtk_label_new("Password"));

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->vbox), notebook, TRUE, TRUE, 0);

  gtk_widget_show(gecos);
  gtk_widget_show(pw_field_old);
  gtk_widget_show(pw_field_1);
  gtk_widget_show(pw_field_2);
  gtk_widget_show(password);
  gtk_widget_show(notebook);
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
create_gecos_table(UserInfo* userinfo, int rows, int cols)
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
    
  gecos = gtk_table_new(rows + 1, cols, FALSE);
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
