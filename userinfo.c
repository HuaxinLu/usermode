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
#include <pwd.h>
#include <sys/types.h>
#include <gtk/gtk.h>

#define NUM_GECOS_FIELDS 4

struct _UserInfo
{
  /* use GString*? */
  char* full_name;
  char* office;
  char* office_phone;
  char* home_phone;
  char* shell;
};

typedef struct _UserInfo UserInfo;

void create_userinfo_window();
GtkWidget* create_shell_menu();
GtkWidget* create_gecos_table(int rows, int cols);
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

  create_userinfo_window();

  gtk_main();

  return 0;

}

void
create_userinfo_window()
{
  GtkWidget* main;		/* GtkWindow */
  GtkWidget* vbox;		/* GtkVBox */
  GtkWidget* notebook;		/* GtkNotebook */
  GtkWidget* gecos;		/* GtkTable */
  GtkWidget* shell_field;	/* GtkOptionMenu */
  GtkWidget* password;		/* GtkVBox */
  GtkWidget* pw_info;		/* GtkLabel */
  GtkWidget* pw_field_old;	/* GtkEntry */
  GtkWidget* pw_field_1;	/* GtkEntry */ /* need gtk_entry_set_echo_char() */
  GtkWidget* pw_field_2;	/* GtkEntry */ /* ditto */
  GtkWidget* seperator;		/* GtkSeperator */
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

/*   vbox = gtk_vbox_new(FALSE, 0); */
/*   vbox = GTK_DIALOG(main)->vbox; */

  notebook = gtk_notebook_new();
  gtk_container_border_width(GTK_CONTAINER(notebook), 5);

  gecos = create_gecos_table(NUM_GECOS_FIELDS, 2);

  shell_field = create_shell_menu();

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

/*   gtk_container_add(GTK_CONTAINER(main), vbox); */
/*   gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0); */
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->vbox), notebook, TRUE, TRUE, 0);
/* gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), TRUE, TRUE, 0); */
/* gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), buttons, TRUE, TRUE, 0); */

  gtk_widget_show(gecos);
  gtk_widget_show(pw_field_old);
  gtk_widget_show(pw_field_1);
  gtk_widget_show(pw_field_2);
  gtk_widget_show(password);
  gtk_widget_show(notebook);
  gtk_widget_show(seperator);
  gtk_widget_show(buttons);
  gtk_widget_show(vbox);
  gtk_widget_show(main);
}

GtkWidget*
create_shell_menu()
{
  GtkWidget* shell_menu;
  GtkWidget* menu;
  GtkWidget* menuitem;

  /* need to actually parse /etc/shells */
  /* need to be able to get the current value... can't do that with
   * gtk option menus, so I need to attach signals to the various
   * menuitems that will set some data.  That means this function
   * should take an argument, and the function called from the signals
   * need to take arguments.. I need to figure out how that's done.
   */

  shell_menu = gtk_option_menu_new();
  menu = gtk_menu_new();

  menuitem = gtk_menu_item_new_with_label("/bin/ash");
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);
  menuitem = gtk_menu_item_new_with_label("/bin/bash");
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);
  menuitem = gtk_menu_item_new_with_label("/bin/sh");
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);
  menuitem = gtk_menu_item_new_with_label("/bin/ksh");
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);
  menuitem = gtk_menu_item_new_with_label("/bin/tcsh");
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);
  menuitem = gtk_menu_item_new_with_label("/bin/zsh");
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);
  menuitem = gtk_menu_item_new_with_label("/bin/bsh");
  gtk_menu_append(GTK_MENU(menu), menuitem);
  gtk_widget_show(menuitem);

  gtk_option_menu_set_menu(GTK_OPTION_MENU(shell_menu), menu);
  return shell_menu;
}

GtkWidget*
create_gecos_table(int rows, int cols)
{
  GtkWidget* gecos;		/* GtkTable */
  GtkWidget* gecos_fields[NUM_GECOS_FIELDS];
  GtkWidget* label;
  int i;

  gecos = gtk_table_new(rows + 1, cols, FALSE);
  gtk_container_border_width(GTK_CONTAINER(gecos), 5);

  gtk_table_set_row_spacings(GTK_TABLE(gecos), 5);
  gtk_table_set_col_spacings(GTK_TABLE(gecos), 5);

  label = gtk_label_new("Full Name:");
  gtk_table_attach(GTK_TABLE(gecos), label, 
		   0, 1, 0, 1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("Office:");
  gtk_table_attach(GTK_TABLE(gecos), label, 
		   0, 1, 1, 2,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("Office Phone:");
  gtk_table_attach(GTK_TABLE(gecos), label, 
		   0, 1, 2, 3,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("Home Phone:");
  gtk_table_attach(GTK_TABLE(gecos), label, 
		   0, 1, 3, 4,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(label);

  label = gtk_label_new("Shell:");
  gtk_table_attach(GTK_TABLE(gecos), label,
		   0, 1, 4, 5,
		   GTK_EXPAND | GTK_FILL,
		   GTK_EXPAND | GTK_FILL,
		   0, 0);
  gtk_widget_show(label);

  for(i = 0; i < rows; i++)
    {
      GtkWidget* field;
      field = gtk_entry_new();
      gtk_widget_show(field);
      gecos_fields[i] = field;
      gtk_table_attach(GTK_TABLE(gecos), gecos_fields[i], 
		       1, 2, 0+i, 1+i,
		       GTK_EXPAND | GTK_FILL, 
		       GTK_EXPAND | GTK_FILL, 
		       0, 0);
      gtk_widget_show(gecos_fields[i]);
    }

  return gecos;
}

UserInfo* 
parse_userinfo()
{
  /* do serious error checking */

  UserInfo* retval;
  struct passwd* pwent;
  char* gecos;
  char pwsep = ':';

  /* use gmalloc()? */
  retval = malloc(sizeof(UserInfo));

  /* need to get this stuff...
   * retval->full_name
   * retval->office
   * retval->office_phone
   * retval->home_phone
   * retval->shell
   */

  pwent = getpwuid(getuid());

  retval->shell = strdup(pwent->pw_shell);
  gecos = strdup(pwent->pw_gecos);

  if(strchr(gecos, pwsep) == NULL)
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
      retval->full_name = strtok(gecos, &pwsep);
      retval->office = strtok(NULL, &pwsep);
      retval->office_phone = strtok(NULL, &pwsep);
      retval->home_phone = strtok(NULL, &pwsep);
    }

  return retval;
}

