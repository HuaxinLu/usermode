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

/* TODO notes.
 * This code is probably not the cleanest code ever written right
 * now.  In fact, I'm sure it's not.  When I have a breather, I'll
 * clean it up a bit... which includes moving some of the tedious gui
 * stuff out into a library of sorts.  I'm sick of coding info and
 * error boxes by hand. :)
 * 
 * Things that would be nifty features, that I'll add one of these
 * days...
 * - swap(on/off) for swap partitions.  Right now the tool ignores
 *   anything with a fstype of swap.  That's pretty much the right
 *   thing to do, for now.
 * - eject button.  Something I'd like to see... I'm not quite clear
 *   on how I could check which devices are ejectable, though.
 *   Clearly, it would require write permissions, but how do I tell if
 *   I'm on a machine with an ejectable floppy or not... interesting
 *   question. 
 */

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <glob.h>
#include <libintl.h>
#include <limits.h>
#include <locale.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include "config.h"
#include "userdialogs.h"

#define N_(String)	String
#define _(String)	gettext(String)
#define MOUNT_TEXT	_("_Mount")
#define UNMOUNT_TEXT	_("Un_mount")
#define PREFERRED_FS	"ext2"
#define ACTION_FORMAT	1
#define ACTION_MOUNT	2
#define PAD		8

/* The structure which holds all of the information needed to process a
 * mount/umount/format request. */
struct mountinfo {
	char *dir;		/* mount point */
	char *dev;		/* device name */
	char *fstype;		/* filesystem type */
	gboolean mounted;	/* TRUE=mounted, FALSE=unmounted */
	gboolean writable;	/* TRUE=+w, FALSE=-w */
	gboolean fdformat;	/* TRUE=should run fdformat, FALSE=can't */
	GtkWidget *mount;	/* mount button */
	GtkWidget *format;	/* format button */
	GtkWidget *mount_label;	/* label in the mount button */
	struct mountinfo *next;	/* linked list... */
};

/* The tree view widget which contains the needed mountinfo struct in its
 * fourth column.  We can't pass this in any other way because a GtkDialog
 * response callback doesn't have a userdata argument.  */
static GtkTreeSelection *tree_selection = NULL;

/* This function parses /etc/fstab using getmntent() and uses other tricks
 * to fill in the info for all filesystems with the 'user' option... unless,
 * of course getuid() == 0, then we get them all.  No reason root shouldn't
 * be allowed to use this, right?.  */
static struct mountinfo *
build_mountinfo_list()
{
	struct mountinfo *ret = NULL;
	struct mountinfo *tmp = NULL;
	char *parent_dir;
	struct mntent *fstab_entry;
	FILE *fstab;

	/* Open the /etc/fstab file (yes, the preprocessor define is named
	 * with an "m", don't let that throw you. */
	fstab = setmntent(_PATH_MNTTAB, "r");
	ret = NULL;

	/* Iterate over all of the entries. */
	while((fstab_entry = getmntent(fstab)) != NULL) {
		struct stat st, st1, st2;
		const char *node, *fstype, *mountpoint;
		char *mkfs, *sbindir;
		gboolean owner, user, swap, superuser;

		/* Read flags. */
		owner = (hasmntopt(fstab_entry, "owner") != NULL);
		user = (hasmntopt(fstab_entry, "user") != NULL);
		swap = (strcmp(fstab_entry->mnt_type, "swap") == 0);
		superuser = (geteuid() == 0);
		node = fstab_entry->mnt_fsname;
		fstype = fstab_entry->mnt_type;
		mountpoint = fstab_entry->mnt_dir;

		/* If the device is in /dev, and the current user owns it,
		 * make note of the fact that we own it. */
		memset(&st, 0, sizeof(st));
		if(owner && (strncmp(node, "/dev/", 5) == 0)) {
			owner = FALSE;
			if(stat(node, &st) != -1) {
				if(st.st_uid == getuid()) {
					owner = TRUE;
				}
			}
		}

		/* We only process this entry if we are allowed to mount it
		 * (it has the "user" flag, or it has the "owner" flag and
		 * we own the device), and it's not a swap partition. */
		if((!user && !owner && !superuser) || swap) {
			continue;
		}

		/* Screen out bogus mount points.  I consider "/" bogus, too. */
		if(mountpoint[0] != '/') {
			continue;
		}
		if(strlen(mountpoint) < 2) {
			continue;
		}
		if(stat(mountpoint, &st1) == -1) {
			continue;
		}

		/* Get the name of the mountpoint's parent directory. */
		parent_dir = g_strdup(mountpoint);
		if(strrchr(parent_dir, '/') == parent_dir) {
			/* The mountpoint is a top-level directory. */
			strcpy(parent_dir, "/");
		} else {
			/* The parent is not the root directory. */
			g_free(parent_dir);
			parent_dir = g_strdup_printf("%s/..", mountpoint);
		}

		/* Verify that the mountpoint and its parent exist. */
		if((stat(mountpoint, &st1) == -1) ||
		   (stat(parent_dir, &st2) == -1)) {
			g_free(parent_dir);
			continue;
		}
		g_free(parent_dir);

		/* Create a new list item and place it at the list's head. */
		tmp = g_malloc0(sizeof(struct mountinfo));
		tmp->next = ret;
		ret = tmp;

		/* Populate the structure. */
		ret->dir = g_strdup(fstab_entry->mnt_dir);
		ret->dev = g_strdup(fstab_entry->mnt_fsname);
		ret->fstype = g_strdup(fstab_entry->mnt_type);

		/* Check if the filesystem is mounted. */
		ret->mounted = (st1.st_dev != st2.st_dev);

		/* Check if we can write to the device.  This indicates that
		 * we can format it and create a filesystem on it. */
		ret->writable = ((access(ret->dev, W_OK) == 0) || superuser);

		/* Now double-check that we have the software to format it. */
		if(strcmp(ret->fstype, "auto") != 0) {
			sbindir = g_path_get_dirname(PATH_MKFS);
			mkfs = g_strdup_printf("%s/mkfs.%s",
					       sbindir, ret->fstype);
			g_free(sbindir);
			if(access(mkfs, X_OK) != 0) {
				ret->writable = FALSE;
			}
			g_free(mkfs);
		}

		/* FIXME: we make a kernel assumption which is NOT PORTABLE to
		 * determine if it's a floppy-disk-type device. */
		ret->fdformat = (S_ISBLK(st.st_mode) &&
				 ((major(st.st_rdev) == 2) ||	/* fdc */
				  (major(st.st_rdev) == 47)));	/* usb */
	}

	endmntent(fstab);

	return ret;
}

/* Return the mountinfo struct associated with the currently-selected row in
 * the TreeView, which is stored in the fourth column in its model. */
static struct mountinfo *
selected_info()
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	struct mountinfo *info = NULL;
	if(gtk_tree_selection_get_selected(GTK_TREE_SELECTION(tree_selection),
					   &model, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(model), &iter,
				   3, &info,
				   -1);
	}
	return info;
}

/* Check if a given mountpoint has a filesystem mounted under it. */
static gboolean
check_is_mounted(struct mountinfo *info)
{
	char *parent_dir;
	struct stat st1, st2;

	/* Screen out bogus mount points.  I consider "/" bogus, too. */
	if(info->dir[0] != '/') {
		return FALSE;
	}
	if(strlen(info->dir) < 2) {
		return FALSE;
	}
	if(stat(info->dir, &st1) == -1) {
		return FALSE;
	}

	/* Get the name of the mountpoint's parent directory. */
	parent_dir = g_strdup(info->dir);
	if(strrchr(parent_dir, '/') == parent_dir) {
		/* The mountpoint is a top-level directory. */
		strcpy(parent_dir, "/");
	} else {
		/* The parent is not the root directory. */
		g_free(parent_dir);
		parent_dir = g_strdup_printf("%s/..", info->dir);
	}

	/* Verify that the mountpoint and its parent exist. */
	if((stat(info->dir, &st1) == -1) ||
	   (stat(parent_dir, &st2) == -1)) {
		g_free(parent_dir);
		return FALSE;
	}
	g_free(parent_dir);

	/* Check if the filesystem is mounted. */
	info->mounted = (st1.st_dev != st2.st_dev);

	return info->mounted;
}

/* If the selection changed, we need to enable/disable specific buttons
 * depending on whether or not the selected filesystem is mounted or not. */
static void
changed_callback(GtkTreeSelection *ignored, gpointer also_ignored)
{
	struct mountinfo *info;
	info = selected_info();
	if(info != NULL) {
		/* Enable/disable buttons based on whether the filesystem is
		 * mounted or not. */
		check_is_mounted(info);
		gtk_widget_set_sensitive(info->format,
					 info->writable && (!info->mounted));
		gtk_button_set_label(GTK_BUTTON(info->mount),
				     info->mounted ?
				     UNMOUNT_TEXT : MOUNT_TEXT);
		gtk_widget_set_sensitive(info->mount, TRUE);
	}
}

static void
format(struct mountinfo *info)
{
	GtkWidget *dialog, *vbox, *format;
	GtkWidget *label, *options, *menu, *item, *table;
	GtkResponseType response;
	char *command[5];
	int status, defaultfs;
	GError *error = NULL;
	char *sbindir = NULL, *mkfs = NULL, *fstype = NULL;
	int i;
	gint child;
	glob_t results;

	/* Verify that the user really wants to do this, because it's
	 * a destructive process. */
	dialog = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_YES_NO,
					_("Are you sure you want to format this"
					  " disk?  You will destroy any data it"
					  " currently contains."));

	/* Create a table to hold some widgets. */
	table = gtk_table_new(2, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), PAD);

	/* If it's the kind of device we low-level format, let the user
	 * disable that step. */
	if(info->fdformat) {
		format = gtk_check_button_new_with_mnemonic(_("Perform a "
							      "_low-level "
							      "format."));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(format), TRUE);
		gtk_widget_show(format);
	} else {
		format = NULL;
	}

	/* If it's a filesystem of type "auto", we also have to let the user
	 * select a filesystem type to format it as. */
	if(strcmp(info->fstype, "auto") == 0) {
		sbindir = g_path_get_dirname(PATH_MKFS);
		mkfs = g_strdup_printf("%s/mkfs.*", sbindir);

		menu = gtk_menu_new();
		gtk_widget_show(menu);

		defaultfs = 0;
		if(glob(mkfs, 0, NULL, &results) == 0) {
			for(i = 0; i < results.gl_pathc; i++) {
				const char *text;
				text = results.gl_pathv[i] + strlen(mkfs) - 1;
				item = gtk_menu_item_new_with_label(text);
				gtk_widget_show(item);
				if(strcmp(text, PREFERRED_FS) == 0) {
					defaultfs = i;
				}
				gtk_menu_shell_append(GTK_MENU_SHELL(menu),
						      item);
			}
		} else {
			memset(&results, 0, sizeof(results));
		}

		options = gtk_option_menu_new();
		gtk_option_menu_set_menu(GTK_OPTION_MENU(options), menu);
		if(defaultfs > 0) {
			gtk_option_menu_set_history(GTK_OPTION_MENU(options),
						    defaultfs);
		}
		gtk_widget_show(options);

		label = gtk_label_new_with_mnemonic(_("Select a _filesystem type to create:"));
		gtk_label_set_mnemonic_widget(GTK_LABEL(label), options);
		gtk_widget_show(label);
	} else {
		options = NULL;
		label = NULL;
	}

	/* Run the dialog. */
	if(format) {
		gtk_table_attach_defaults(GTK_TABLE(table), format,
					  0, 2, 0, 1);
	}
	if(options && label) {
		gtk_table_attach_defaults(GTK_TABLE(table), label,
					  0, 1, 1, 2);
		gtk_table_attach_defaults(GTK_TABLE(table), options,
					  1, 2, 1, 2);
	}
	gtk_widget_show_all(table);
	vbox = (GTK_DIALOG(dialog))->vbox;
	gtk_box_pack_start_defaults(GTK_BOX(vbox), table);

	response = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_hide(dialog);

	/* If the user said "yes", make a new filesystem on it. */
	if(response == GTK_RESPONSE_YES) {
		if(GTK_IS_CHECK_BUTTON(format)) {
			GtkToggleButton *toggle = GTK_TOGGLE_BUTTON(format);
			if(gtk_toggle_button_get_active(toggle)) {
				command[0] = PATH_FDFORMAT;
				command[1] = info->dev;
				command[2] = NULL;
				if(g_spawn_async("/",
						 command, NULL,
						 G_SPAWN_DO_NOT_REAP_CHILD |
						 G_SPAWN_STDOUT_TO_DEV_NULL,
						 NULL, NULL,
						 &child, &error)) {
					while(waitpid(child, &status, WNOHANG) == 0) {
						gtk_main_iteration_do(FALSE);
					}
					if(WIFEXITED(status)) {
						if(WEXITSTATUS(status) == 0) {
							info->mounted = !info->mounted;
						}
					}
				}
			}
		}
		fstype = NULL;
		if(options) {
			defaultfs = gtk_option_menu_get_history(GTK_OPTION_MENU(options));
			if(results.gl_pathc > defaultfs) {
				fstype = results.gl_pathv[defaultfs] + strlen(mkfs) - 1;
			}
			g_free(mkfs);
			g_free(sbindir);
		}
		if(fstype == NULL) {
			fstype = info->fstype;
		}
		command[0] = PATH_MKFS;
		command[1] = "-t";
		command[2] = fstype;
		command[3] = info->dev;
		command[4] = NULL;
		if(g_spawn_async("/",
				 command, NULL,
				 G_SPAWN_DO_NOT_REAP_CHILD |
				 G_SPAWN_STDOUT_TO_DEV_NULL,
				 NULL, NULL,
				 &child, &error)) {
			while(waitpid(child, &status, WNOHANG) == 0) {
				gtk_main_iteration_do(FALSE);
			}
			if(WIFEXITED(status)) {
				if(WEXITSTATUS(status) == 0) {
					info->mounted = !info->mounted;
				}
			}
		}
	}

	gtk_widget_destroy(dialog);
}

static void
response_callback(GtkWidget *emitter, GtkResponseType response)
{
	struct mountinfo *info;
	char *command[3];
	int status;
	gint child;
	GError *error = NULL;
	GtkWidget *toplevel;

	info = selected_info();
	switch(response) {
		case GTK_RESPONSE_CLOSE:
			gtk_main_quit();
			break;
		case ACTION_FORMAT:
			/* Format. */
			toplevel = gtk_widget_get_toplevel(emitter);
			if(GTK_WIDGET_TOPLEVEL(toplevel)) {
				gtk_widget_set_sensitive(toplevel, FALSE);
			}
			format(info);
			if(GTK_WIDGET_TOPLEVEL(toplevel)) {
				gtk_widget_set_sensitive(toplevel, TRUE);
			}
			break;
		case ACTION_MOUNT:
			/* Mount/unmount. */
			command[0] = info->mounted ?  PATH_UMOUNT : PATH_MOUNT;
			command[1] = info->dir;
			command[2] = NULL;
			if(g_spawn_async("/",
					 command, NULL,
					 G_SPAWN_DO_NOT_REAP_CHILD |
					 G_SPAWN_STDOUT_TO_DEV_NULL,
					 NULL, NULL,
					 &child, &error)) {
				while(waitpid(child, &status, WNOHANG) == 0) {
					gtk_main_iteration_do(FALSE);
				}
				if(WIFEXITED(status)) {
					if(WEXITSTATUS(status) == 0) {
						info->mounted = !info->mounted;
					}
				}
			}
			changed_callback(NULL, NULL);
			break;
		default:
			break;
	}
}

static void
create_usermount_window()
{
	GtkWidget *dialog, *treeview, *format, *mount;
	GtkTreeIter iter;
	GtkListStore *model;
	GtkTreeViewColumn *column[3];
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	struct mountinfo *info, *i;

	/* First, create a list of filesystems which are configured on this
	 * system, by scanning /etc/fstab.  If we get an empty list back,
	 * bail out and just display an error dialog. */
	info = build_mountinfo_list();
	if(info == NULL) {
		dialog = create_error_box(_("There are no filesystems which you are allowed to mount or unmount.\nContact your administrator."),
					  _("User Mount Tool"));
		g_signal_connect_object(G_OBJECT(dialog), "destroy",
					GTK_SIGNAL_FUNC(gtk_main_quit), NULL,
					G_CONNECT_AFTER);
		return;
	}

	/* Create the dialog. */
	dialog = gtk_dialog_new_with_buttons(_("User Mount Tool"),
					     NULL,
					     0,
					     GTK_STOCK_CLOSE,
					     GTK_RESPONSE_CLOSE,
					     NULL);

	/* Connect default signal handlers. */
	g_signal_connect(G_OBJECT(dialog), "destroy",
			 GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(dialog), "delete_event",
			 GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	/* Create the other buttons. */
	format = gtk_button_new_with_mnemonic(_("_Format"));
	gtk_widget_set_sensitive(format, FALSE);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), format, ACTION_FORMAT);
	mount = gtk_button_new_with_mnemonic(info->mounted ?
		      			     UNMOUNT_TEXT : MOUNT_TEXT);
	gtk_widget_set_sensitive(mount, FALSE);
	gtk_dialog_add_action_widget(GTK_DIALOG(dialog), mount, ACTION_MOUNT);

	/* Set a signal handler to handle button presses. */
	g_signal_connect(G_OBJECT(dialog), "response",
			 GTK_SIGNAL_FUNC(response_callback), NULL);

	/* Build a tree model with this data. */
	model = gtk_list_store_new(4,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_POINTER);

	/* Now add a row to the list using the data. */
	for(i = info; i != NULL; i = i->next) {
		i->format = format;
		i->mount = mount;
		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter,
				   0, i->dev,
				   1, i->dir,
				   2, i->fstype,
				   3, i,
				   -1);
	}

	/* Now add the columns to the tree view. */
	column[0] = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_set_title(column[0], _("Device"));
	gtk_tree_view_column_pack_start(column[0], renderer, TRUE);
	gtk_tree_view_column_add_attribute(column[0], renderer, "text", 0);

	column[1] = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_set_title(column[1], _("Directory"));
	gtk_tree_view_column_pack_start(column[1], renderer, TRUE);
	gtk_tree_view_column_add_attribute(column[1], renderer, "text", 1);

	column[2] = gtk_tree_view_column_new();
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_set_title(column[2], _("Filesystem"));
	gtk_tree_view_column_pack_start(column[2], renderer, TRUE);
	gtk_tree_view_column_add_attribute(column[2], renderer, "text", 2);

	/* Create the tree view and add the columns to it. */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	g_object_unref(G_OBJECT(model));
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column[0]);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column[1]);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column[2]);

	/* Set up a callback for when the selection changes. */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	tree_selection = selection;
	g_signal_connect(G_OBJECT(selection), "changed",
		         GTK_SIGNAL_FUNC(changed_callback), NULL);

	/* Pack it in and show the dialog. */
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), treeview,
			   TRUE, TRUE, 0);

	gtk_widget_show_all(dialog);
}

int
main(int argc, char *argv[])
{
	bindtextdomain(PACKAGE, DATADIR "/locale");
	textdomain(PACKAGE);
	gtk_set_locale();
	gtk_init(&argc, &argv);
	glade_init();
	create_usermount_window();
	gtk_main();
	return 0;
}
