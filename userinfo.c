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
extern char **environ;

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
		  *shell_menu, *window = NULL;
	char *shell;
	gboolean saw_shell = FALSE;

	xml = glade_xml_new(DATADIR "/" PACKAGE "/" PACKAGE ".glade",
			    "userinfo", PACKAGE);
	if (xml) {
		window = glade_xml_get_widget(xml, "userinfo");
		g_assert(window != NULL);

		gtk_window_set_icon_from_file(GTK_WINDOW(window), "/usr/share/pixmaps/user_icon.png", NULL);

		g_object_set_data(G_OBJECT(window),
				  USERINFO_DATA_NAME, userinfo);
		g_object_set_data(G_OBJECT(window),
				  USERINFO_XML_NAME, xml);
		g_signal_connect(window, "destroy",
				 G_CALLBACK(userhelper_main_quit), window);

		entry = glade_xml_get_widget(xml, "fullname");
		g_assert(entry != NULL);
		if (GTK_IS_ENTRY(entry) && userinfo->full_name) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					   userinfo->full_name);
		}

		entry = glade_xml_get_widget(xml, "office");
		g_assert(entry != NULL);
		if (GTK_IS_ENTRY(entry) && userinfo->office) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					   userinfo->office);
		}

		entry = glade_xml_get_widget(xml, "officephone");
		g_assert(entry != NULL);
		if (GTK_IS_ENTRY(entry) && userinfo->office_phone) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					   userinfo->office_phone);
		}

		entry = glade_xml_get_widget(xml, "homephone");
		g_assert(entry != NULL);
		if (GTK_IS_ENTRY(entry) && userinfo->home_phone) {
			gtk_entry_set_text(GTK_ENTRY(entry),
					   userinfo->home_phone);
		}

		shell_menu = glade_xml_get_widget(xml, "shellmenu");
		g_assert(shell_menu != NULL);
		menu = gtk_menu_new();

		setusershell();
		while ((shell = getusershell()) != NULL) {
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
			if (strcmp(shell, userinfo->shell) == 0) {
				gtk_menu_reorder_child(GTK_MENU(menu), item, 0);
				saw_shell = TRUE;
			}
		}
		if (!saw_shell) {
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

		widget = glade_xml_get_widget(xml, "apply");
		g_assert(widget != NULL);
		g_signal_connect(widget, "clicked",
				 G_CALLBACK(on_ok_clicked), window);
		widget = glade_xml_get_widget(xml, "close");
		g_assert(widget != NULL);
		g_signal_connect(widget, "clicked",
				 G_CALLBACK(userhelper_main_quit), window);
	}

	return widget;
}

static struct UserInfo *
parse_userinfo(void)
{
	struct UserInfo *retval;
	struct passwd *pwent;

	char **vals;

	pwent = getpwuid(getuid());
	if (pwent == NULL) {
		return NULL;
	}
	retval = g_malloc0(sizeof(struct UserInfo));

	retval->shell = g_strdup(pwent->pw_shell);
	vals = g_strsplit(pwent->pw_gecos ?: "", ",", 5);
	if (vals != NULL) {
		if (vals[0]) {
			retval->full_name = g_strdup(vals[0]);
		}
		if (vals[0] && vals[1]) {
			retval->office = g_strdup(vals[1]);
		}
		if (vals[0] && vals[1] && vals[2]) {
			retval->office_phone = g_strdup(vals[2]);
		}
		if (vals[0] && vals[1] && vals[2] && vals[3]) {
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
	if (!GTK_WIDGET_TOPLEVEL(toplevel)) {
		return FALSE;
	}
	userinfo = g_object_get_data(G_OBJECT(toplevel),
				     USERINFO_DATA_NAME);
	xml = g_object_get_data(G_OBJECT(toplevel), USERINFO_XML_NAME);

	entry = glade_xml_get_widget(xml, "fullname");
	if (GTK_IS_ENTRY(entry)) {
		userinfo->full_name = gtk_entry_get_text(GTK_ENTRY(entry));
	}

	entry = glade_xml_get_widget(xml, "office");
	if (GTK_IS_ENTRY(entry)) {
		userinfo->office = gtk_entry_get_text(GTK_ENTRY(entry));
	}

	entry = glade_xml_get_widget(xml, "officephone");
	if (GTK_IS_ENTRY(entry)) {
		userinfo->office_phone = gtk_entry_get_text(GTK_ENTRY(entry));
	}

	entry = glade_xml_get_widget(xml, "homephone");
	if (GTK_IS_ENTRY(entry)) {
		userinfo->home_phone = gtk_entry_get_text(GTK_ENTRY(entry));
	}

	if (GTK_IS_WIDGET(toplevel)) {
		gtk_widget_set_sensitive(GTK_WIDGET(toplevel), FALSE);
	}
	set_new_userinfo(userinfo);
	if (GTK_IS_WIDGET(toplevel)) {
		gtk_widget_set_sensitive(GTK_WIDGET(toplevel), TRUE);
	}
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
	char *argv[12];
	int i = 0;

	fullname = userinfo->full_name;
	office = userinfo->office;
	officephone = userinfo->office_phone;
	homephone = userinfo->home_phone;
	shell = userinfo->shell;

	argv[i++] = UH_PATH;

	if (fullname) {
		argv[i++] = UH_FULLNAME_OPT;
		argv[i++] = (char *)(fullname ?: "");
	}

	if (office) {
		argv[i++] = UH_OFFICE_OPT;
		argv[i++] = (char *)(office ?: "");
	}

	if (officephone) {
		argv[i++] = UH_OFFICEPHONE_OPT;
		argv[i++] = (char *)(officephone ?: "");
	}

	if (homephone) {
		argv[i++] = UH_HOMEPHONE_OPT;
		argv[i++] = (char *)homephone;
	}

	if (shell) {
		argv[i++] = UH_SHELL_OPT;
		argv[i++] = (char *)shell;
	}

	argv[i++] = NULL;

	userhelper_runv(TRUE, UH_PATH, argv);
}

void
userhelper_fatal_error(int ignored)
{
	userhelper_main_quit();
}

static int
safe_strcmp (const char *s1, const char *s2)
{
        return strcmp (s1 ? s1 : "", s2 ? s2 : "");
}

static void
parse_args (struct UserInfo *userinfo, int argc, char *argv[])
{
	int changed;
	int x_flag;
	int arg;

        changed = 0;
        x_flag = 0;

   	while ((arg = getopt(argc, argv, "f:o:p:h:s:x")) != -1) {
                switch (arg) {
                        case 'f':
                                /* Full name. */
				if (safe_strcmp (userinfo->full_name, optarg) != 0) {
	                                changed = 1;
                                	userinfo->full_name = optarg;
				}
                                break;
                        case 'o':
                                /* Office. */
				if (safe_strcmp (userinfo->office, optarg) != 0) {
	                                changed = 1;
                                	userinfo->office = optarg;
				}
                                break;
                        case 'h':
                                /* Home phone. */
				if (safe_strcmp (userinfo->home_phone, optarg) != 0) {
	                                changed = 1;
                                	userinfo->home_phone = optarg;
				}
                                break;
                        case 'p':
                                /* Office phone. */
				if (safe_strcmp (userinfo->office_phone, optarg) != 0) {
	                                changed = 1;
                                	userinfo->office_phone = optarg;
				}
                                break;
                        case 's':
                                /* Shell. */
				if (safe_strcmp (userinfo->shell, optarg) != 0) {
	                                changed = 1;
                                	userinfo->shell = optarg;
				}
                                break;
                        case 'x':
				x_flag = 1;
				break;
			default:
				fprintf(stderr, _("Unexpected argument"));
				exit(1);
		}
	}

	if (x_flag) {
		if (changed)
			set_new_userinfo(userinfo);

		exit(0);
	}
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
	if (userinfo == NULL) {
		fprintf(stderr, _("You don't exist.  Go away.\n"));
		exit(1);
	}

	gtk_init(&argc, &argv);

	glade_init();

	parse_args (userinfo, argc, argv);

	window = create_userinfo_window(userinfo);
	gtk_widget_show_all(window);

#ifdef DEBUG_USERHELPER
	fprintf(stderr, "Running.\n");
#endif

	gtk_main();

#ifdef DEBUG_USERHELPER
	fprintf(stderr, "Exiting.\n");
#endif

	return 0;
}
