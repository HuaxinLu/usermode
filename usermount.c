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

#include <stdio.h>
#include <mntent.h>
#include <gtk/gtk.h>

/* general thoughts on the usermount tool...
 * -- needs to system() out to /bin/mount.  I need to be suid root to
 *    actualy mount, and I don't want to do that.
 * -- may want to check write on the device, so I can throw up a
 *    message saying that the user won't be able to format floppies.
 * -- want to parse /etc/fstab (it's always readable, right?) to get a
 *    list of the user mountable filesystems.  List those as things to
 *    mount/umount
 */
/* interface thoughts...
 * A listbox might be a good idea, but there shouldn't be a whole lot
 * of user mountable filesystems... so a dynamically generated table
 * would be good.  Something like this:
 * mountpoint device fstype button
 * The button is a toggle button... mount or umount... or maybe a
 * button that changes it's label.  Need to figure out how to get
 * current mount status of filesystems.
 */

void create_usermount_window();
GtkWidget* create_mount_table();

int
main(int argc, char* argv[])
{
  MountInfo* mountinfo

  /* gtk_set_locale(); */		/* this is new... */
  gtk_init(&argc, &argv);
  /* put this back in when I've decided I need it... */
  /*   gtk_rc_parse("userinforc"); */

  create_usermount_window();

  gtk_main();

  return 0;

}

void
create_usermount_window(MountInfo* mountinfo)
{
  GtkWidget* main;
  GtkWidget* ok;
  GtkWidget* cancel;
  GtkWidget* help;
  GtkWidget* mount_table;
  
  main = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(main), "User Mount Tool")
  gtk_signal_connect(GTK_OBJECT(main), "destroy",
		     (GtkSignalFunc) gtk_exit, NULL);

  /* action_area buttons */
  ok = gtk_button_new_with_label("OK");
  cancel = gtk_button_new_with_label("Cancel");
  help = gtk_button_new_with_label("Help");

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     ok, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     cancel, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     help, FALSE, FALSE, 0);

  mount_table = create_mount_table(mountinfo);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->vbox), mount_table,
		     TRUE, TRUE, 0);

  gtk_widget_show(mount_table);
  gtk_widget_show(ok);
  gtk_widget_show(cancel);
  gtk_widget_show(help);


}

GtkWidget*
create_mount_table(MountInfo* mountinfo)
{
  GtkWidget* mount_table;
  GtkWidget* mountpoint;
  GtkWidget* device;
  GtkWidget* fstype;
  GtkWidget* mount;

  struct mntent* fstab_entry;
  struct mntent* mtab_entry;
  FILE* fstab;
/*   FILE* mtab; */
  int row = 0;

a  /* hardcoded limit of 20 user mountable filesystems.  Shouldn't ever
   * get that high, but it's possible... it will be much easier to fix
   * when gtk tables can resize themselves, or something of the sort.
   */
  mount_table = gtk_table_new(20, 4, FALSE);

  /* this is a little trickier than I had hoped... it's going to be
   * hard to figure out the current status of the filesystems... it
   * can be done, but I'm not going to code it right now.
   *
   * Also, getmntent uses static memory, so I'm going to have to
   * figure out the status after building the table.  Ugh!
   */

  fstab = setmntent(MNTTAB, 'r');
/*   mtab = setmntent("/etc/mtab", 'r'); */

  do
    {
      fstab_entry = getmntent(fstab);

      /* not sure if user is a valid "real" option... I think it might
       * just be translated into equivalent other options... reason is
       * that mntent.h doesn't have a macro for it.  We'll see.
       */
      if(hasmntopt(fstab_entry, "user") != NULL)
	{
	  mountpoint = gtk_label_new(fstab_entry->mnt_dir);
	  gtk_table_attach(GTK_TABLE(mount_table), mountpoint, 
			   0, 1, row, row+1,
			   GTK_EXPAND | GTK_FILL, 
			   GTK_EXPAND | GTK_FILL, 
			   0, 0);
	  gtk_widget_show(mountpoint);

	  /* not sure that mnt_fsname is the "Right Thing" */
	  device = gtk_label_new(fstab_entry->mnt_fsname);
	  gtk_table_attach(GTK_TABLE(mount_table), device, 
			   1, 2, row, row+1,
			   GTK_EXPAND | GTK_FILL, 
			   GTK_EXPAND | GTK_FILL, 
			   0, 0);
	  gtk_widget_show(device);
	  
	  fstype = gtk_label_new(fstab_entry->mnt_type);
	  gtk_table_attach(GTK_TABLE(mount_table), fstype, 
			   2, 3, row, row+1,
			   GTK_EXPAND | GTK_FILL, 
			   GTK_EXPAND | GTK_FILL, 
			   0, 0);
	  gtk_widget_show(fstype);

	  mount = gtk_toggle_button_new_with_label("Mount");
	  gtk_table_attach(GTK_TABLE(mount_table), mount, 
			   3, 4, row, row+1,
			   GTK_EXPAND | GTK_FILL, 
			   GTK_EXPAND | GTK_FILL, 
			   0, 0);
	  gtk_widget_show(mount);

 
	  row++;
	}
    }
  while(fstab_entry != NULL);

  return mount_table;
}

