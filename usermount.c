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
#include <stdlib.h>
#include <unistd.h>
#include <mntent.h>
#include <string.h>
#include <wait.h>
#include <errno.h>
#include <sys/stat.h>
#include <gtk/gtk.h>

#define MAXLINE 512

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
void user_mount(GtkWidget* widget, char* file);

int
main(int argc, char* argv[])
{
  /* gtk_set_locale(); */		/* this is new... */
  gtk_init(&argc, &argv);
  /* put this back in when I've decided I need it... */
  /*   gtk_rc_parse("userinforc"); */

  create_usermount_window();

  gtk_main();

  return 0;

}

void
create_usermount_window()
{
  GtkWidget* main;
  GtkWidget* ok;
  GtkWidget* cancel;
  GtkWidget* help;
  GtkWidget* mount_table;
  
  main = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(main), "User Mount Tool");
  gtk_signal_connect(GTK_OBJECT(main), "destroy",
		     (GtkSignalFunc) gtk_exit, NULL);

  /* action_area buttons */
  ok = gtk_button_new_with_label("OK");
  cancel = gtk_button_new_with_label("Cancel");
  gtk_signal_connect(GTK_OBJECT(cancel), "clicked", 
		     (GtkSignalFunc) gtk_exit, NULL);
  help = gtk_button_new_with_label("Help");

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     ok, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     cancel, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->action_area), 
		     help, FALSE, FALSE, 0);

  mount_table = create_mount_table();

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(main)->vbox), mount_table,
		     TRUE, TRUE, 0);

  gtk_widget_show(mount_table);
  gtk_widget_show(ok);
  gtk_widget_show(cancel);
  gtk_widget_show(help);
  gtk_widget_show(main);

}

GtkWidget*
create_mount_table()
{
  GtkWidget* mount_table;
  GtkWidget* mountpoint;
  GtkWidget* device;
  GtkWidget* fstype;
  GtkWidget* mount;
  GtkWidget* headings;

  struct mntent* fstab_entry;
/*   struct mntent* mtab_entry; */
  FILE* fstab;
/*   FILE* mtab; */
  int row = 0;

  /* hardcoded limit of 20 user mountable filesystems.  Shouldn't ever
   * get that high, but it's possible... it will be much easier to fix
   * when gtk tables can resize themselves, or something of the sort.
   */
  mount_table = gtk_table_new(5, 4, FALSE);
  gtk_container_border_width(GTK_CONTAINER(mount_table), 5);
  gtk_table_set_row_spacings(GTK_TABLE(mount_table), 5);
  gtk_table_set_col_spacings(GTK_TABLE(mount_table), 5);

  headings = gtk_label_new("Mount Point");
  gtk_table_attach(GTK_TABLE(mount_table), headings,
		   0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(headings);

  headings = gtk_label_new("Device");
  gtk_table_attach(GTK_TABLE(mount_table), headings,
		   1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(headings);

  headings = gtk_label_new("Type");
  gtk_table_attach(GTK_TABLE(mount_table), headings,
		   2, 3, row, row+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(headings);
  

  row++;
  /* this is a little trickier than I had hoped... it's going to be
   * hard to figure out the current status of the filesystems... it
   * can be done, but I'm not going to code it right now.
   *
   * Also, getmntent uses static memory, so I'm going to have to
   * figure out the status after building the table.  Ugh!
   */

  fstab = setmntent(MNTTAB, "r");
/*   mtab = setmntent("/etc/mtab", 'r'); */

  while((fstab_entry = getmntent(fstab)) != NULL)
    {
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

/* 	  mount = gtk_toggle_button_new_with_label("Mount"); */
	  mount = gtk_button_new_with_label("Mount");
	  /* FIXME: need to pass an arg... */
	  gtk_signal_connect(GTK_OBJECT(mount), "clicked",
			     (GtkSignalFunc) user_mount, 
			     strdup(fstab_entry->mnt_dir));
	  gtk_table_attach(GTK_TABLE(mount_table), mount, 
			   3, 4, row, row+1,
			   GTK_EXPAND | GTK_FILL, 
			   GTK_EXPAND | GTK_FILL, 
			   0, 0);
	  gtk_widget_show(mount);

 
	  row++;
	}
    }

  return mount_table;
}

void
user_mount(GtkWidget* widget, char* file)
{
  /* more aggressive error checking. */

  char* cmd = "/bin/mount";
  char* cmd_buf;
  int cmd_len;
  int tmperr;
  int tmpout;
  pid_t pid;
  int count;
  struct stat stat_buf;

/*   char outline[MAXLINE]; */
/*   char errline[MAXLINE]; */
  char* errline;
  char* outline;

  tmperr = mkstemp(strdup("errXXXXXX"));
  tmpout = mkstemp(strdup("outXXXXXX"));

  if((pid = fork()) < 0)
    {
      fprintf(stderr, "Cannot fork().\n");
    }
  else if(pid > 0)		/* parent */
    {
     if(waitpid(pid, NULL, 0) < 0)
	{
	  fprintf(stderr, "waitpid() error\n");
	  exit(2);
	}

     fstat(tmperr, &stat_buf);
     if(stat_buf.st_size > 0)
       {
	 errline = malloc(sizeof(char) * stat_buf.st_size + 1);
	 if(read(tmperr, errline, stat_buf.st_size) !=
	    stat_buf.st_size)
	   {
	     fprintf(stderr, "read() error, errno=%d.", errno);
	     exit(2);
	   }
	 printf("errline is: %s", errline);
       }

     fstat(tmpout, &stat_buf);
     if(stat_buf.st_size > 0)
       {
	 outline = malloc(sizeof(char) * stat_buf.st_size + 1);
	 if(read(tmpout, outline, stat_buf.st_size) !=
	    stat_buf.st_size)
	   {
	     fprintf(stderr, "read() errord, errno=%d.", errno);
	     exit(2);
	   }
	 printf("outline is: %s", outline);
       }

     close(tmperr);
     close(tmpout);

     return;
    }
  else				/* child */
    {
      cmd_len = strlen(cmd) + strlen(file) + 2;
      cmd_buf = malloc(sizeof(char) * cmd_len);
      snprintf(cmd_buf, cmd_len, "%s %s", cmd, file);	

      if(tmpout != STDOUT_FILENO)
	{
	  if(dup2(tmpout, STDOUT_FILENO) != STDOUT_FILENO)
	    {
	      fprintf(stdout, "dup2() error.\n");
	      exit(2);
	    }
	}
      if(tmperr != STDERR_FILENO)
	{
	  if(dup2(tmperr, STDERR_FILENO) != STDERR_FILENO)
	    {
	      fprintf(stdout, "dup2() error.\n");
	      exit(2);
	    }
	}

      system(cmd_buf);
      fprintf(stderr, "Get some output on stderr.\n");
      fprintf(stdout, "Get some output on stdout.\n");
      fprintf(stdout, "Get some more output on stdout.\n");
    }

  printf("Leaving user_mount().");

}
