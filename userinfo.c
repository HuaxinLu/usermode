/*
 * Copyright (C) 1997 Red Hat Software, Inc.
 * Copyright (C) 2001 Red Hat, Inc.
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

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <libintl.h>
#include <unistd.h>
#include <fcntl.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <glade/glade.h>
#include <glade/glade-xml.h>
#include <gtk/gtk.h>
#include "userdialogs.h"
#include "userhelper-wrap.h"

#define USERINFO_DATA_NAME "userinfo-data"
#define USERINFO_XML_NAME "userinfo-xml"
struct UserInfo {
	const char *full_name;
	const char *office;
	const char *office_phone;
	const char *home_phone;
	const char *shell;
};

static void set_new_userinfo(struct UserInfo *userinfo);
static gint on_ok_clicked(GtkWidget *widget, gpointer data);

static void
shell_activate(GtkWidget *widget, gpointer data)
{
	struct UserInfo *userinfo;

	userinfo = g_object_get_data(G_OBJECT(widget), USERINFO_DATA_NAME);

	userinfo->shell = (char*)data;
}

static GtkWidget *
create_userinfo_window(struct UserInfo *userinfo)
{
	GladeXML *xml = NULL;
	GtkWidget *widget = NULL, *entry = NULL, *menu = NULL, *item = NULL,
		  *shell_menu;
	char *shell;
	gboolean saw_shell = FALSE;

	xml = glade_xml_new(DATADIR "/" PACKAGE "/" PACKAGE ".glade",
			    "userinfo", PACKAGE);
	if(xml) {
		widget = glade_xml_get_widget(xml, "userinfo");
		g_object_set_data(G_OBJECT(widget),
				  USERINFO_DATA_NAME, userinfo);
		g_object_set_data(G_OBJECT(widget),
				  USERINFO_XML_NAME, xml);

		entry = glade_xml_get_widget(xml, "fullname");
		if(GTK_IS_ENTRY(entry) && userinfo->full_name) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					   userinfo->full_name);
		}

		entry = glade_xml_get_widget(xml, "office");
		if(GTK_IS_ENTRY(entry) && userinfo->office) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					   userinfo->office);
		}

		entry = glade_xml_get_widget(xml, "officephone");
		if(GTK_IS_ENTRY(entry) && userinfo->office_phone) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					   userinfo->office_phone);
		}

		entry = glade_xml_get_widget(xml, "homephone");
		if(GTK_IS_ENTRY(entry) && userinfo->home_phone) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					   userinfo->home_phone);
		}

		shell_menu = glade_xml_get_widget(xml, "shellmenu");
		menu = gtk_menu_new();

		setusershell();
		while((shell = getusershell()) != NULL) {
			/* Filter out "nologin" to keep the user from shooting
			 * self in foot, or similar analogy. */
			if (strstr(shell, "/nologin") != NULL) {
				continue;
			}
			item = gtk_menu_item_new_with_label(shell);
			gtk_widget_show(item);
			g_object_set_data(G_OBJECT(item),
					  USERINFO_DATA_NAME, userinfo);
			g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(shell_activate),
					 g_strdup(shell));
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			if(strcmp(shell, userinfo->shell) == 0) {
				gtk_menu_reorder_child(GTK_MENU(menu), item, 0);
				saw_shell = TRUE;
			}
		}
		if(!saw_shell) {
			item = gtk_menu_item_new_with_label(userinfo->shell);
			gtk_widget_show(item);
			g_object_set_data(G_OBJECT(item),
					  USERINFO_DATA_NAME, userinfo);
			g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(shell_activate),
					 g_strdup(userinfo->shell));
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			gtk_menu_reorder_child(GTK_MENU(menu), item, 0);
		}
		gtk_option_menu_set_menu(GTK_OPTION_MENU(shell_menu), menu);
		gtk_option_menu_set_history(GTK_OPTION_MENU(shell_menu), 0);
		endusershell();

		widget = glade_xml_get_widget(xml, "ok");
		g_signal_connect(widget, "clicked",
				 G_CALLBACK(on_ok_clicked), NULL);
		widget = glade_xml_get_widget(xml, "cancel");
		g_signal_connect(widget, "clicked",
				 G_CALLBACK(gtk_main_quit), NULL);
	}

	return widget;
}

static struct UserInfo *
parse_userinfo()
{
	struct UserInfo *retval;
	struct passwd *pwent;

	char **vals;

	pwent = getpwuid(getuid());
	if(pwent == NULL) {
		return NULL;
	}
	retval = g_malloc0(sizeof(struct UserInfo));

	retval->shell = g_strdup(pwent->pw_shell);
	vals = g_strsplit(pwent->pw_gecos ?: "", ",", 4);
	if(vals != NULL) {
		if(vals[0]) {
			retval->full_name = g_strdup(vals[0]);
		}
		if(vals[0] && vals[1]) {
			retval->office = g_strdup(vals[1]);
		}
		if(vals[0] && vals[1] && vals[2]) {
			retval->office_phone = g_strdup(vals[2]);
		}
		if(vals[0] && vals[1] && vals[2] && vals[3]) {
			retval->home_phone = g_strdup(vals[3]);
		}
		g_strfreev(vals);
	}

	return retval;
}

static gint
on_ok_clicked(GtkWidget *widget, gpointer data)
{
	struct UserInfo *userinfo;
	GtkWidget *toplevel, *entry;
	GladeXML *xml;

	toplevel = gtk_widget_get_toplevel(widget);
	if(!GTK_WIDGET_TOPLEVEL(toplevel)) {
		return FALSE;
	}
	userinfo = g_object_get_data(G_OBJECT(toplevel),
				     USERINFO_DATA_NAME);
	xml = g_object_get_data(G_OBJECT(toplevel), USERINFO_XML_NAME);

	entry = glade_xml_get_widget(xml, "fullname");
	if(GTK_IS_ENTRY(entry)) {
		userinfo->full_name = gtk_entry_get_text(GTK_ENTRY(entry));
	}

	entry = glade_xml_get_widget(xml, "office");
	if(GTK_IS_ENTRY(entry)) {
		userinfo->office = gtk_entry_get_text(GTK_ENTRY(entry));
	}

	entry = glade_xml_get_widget(xml, "officephone");
	if(GTK_IS_ENTRY(entry)) {
		userinfo->office_phone = gtk_entry_get_text(GTK_ENTRY(entry));
	}

	entry = glade_xml_get_widget(xml, "homephone");
	if(GTK_IS_ENTRY(entry)) {
		userinfo->home_phone = gtk_entry_get_text(GTK_ENTRY(entry));
	}

	set_new_userinfo(userinfo);
	return FALSE;
}

static void
set_new_userinfo(struct UserInfo *userinfo)
{
	const char *fullname;
	const char *office;
	const char *officephone;
	const char *homephone;
	const char *shell;
	const char *argv[12];
	int i = 0;

	fullname = userinfo->full_name;
	office = userinfo->office;
	officephone = userinfo->office_phone;
	homephone = userinfo->home_phone;
	shell = userinfo->shell;

	argv[i++] = UH_PATH;

	if(fullname) {
		argv[i++] = UH_FULLNAME_OPT;
		argv[i++] = fullname ?: "";
	}

	if(office) {
		argv[i++] = UH_OFFICE_OPT;
		argv[i++] = office ?: "";
	}

	if(officephone) {
		argv[i++] = UH_OFFICEPHONE_OPT;
		argv[i++] = officephone ?: "";
	}

	if(homephone) {
		argv[i++] = UH_HOMEPHONE_OPT;
		argv[i++] = homephone;
	}

	if(shell) {
		argv[i++] = UH_SHELL_OPT;
		argv[i++] = shell;
	}

	argv[i++] = NULL;

	userhelper_sigchld(0);
	userhelper_runv(UH_PATH, argv);
}

void
userhelper_fatal_error(int ignored)
{
	gtk_main_quit();
}

int
main(int argc, char *argv[])
{
	struct UserInfo *userinfo;
	GtkWidget *window;

	bindtextdomain(PACKAGE, DATADIR "/locale");
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	gtk_set_locale();

	userinfo = parse_userinfo();
	if(userinfo == NULL) {
		fprintf(stderr, _("You don't exist.  Go away.\n"));
		exit(1);
	}

	gtk_init(&argc, &argv);

	glade_init();

	window = create_userinfo_window(userinfo);
	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}
