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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mntent.h>
#include <string.h>
#include <wait.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include "userdialogs.h"

/* max lines in /etc/fstab with the user option... should be big enough. */
#define MAXFSTAB 30
#define MAXLINE 512

#define MOUNT_TEXT N_("Mount")
#define UMOUNT_TEXT N_("Unmount")
#define N_(String) String

struct mountinfo
{
  char* mi_dir;			/* mount point */
  char* mi_dev;			/* device name */
  char* mi_fstype;		/* filesystem type */
  int mi_status;		/* TRUE=mounted, FALSE=unmounted */
  int mi_writable;		/* TRUE=+w, FALSE=-w */
  int mi_fdformat;		/* TRUE=run fdformat, FALSE=don't */
  GtkWidget* mount;		/* mount button */
  GtkWidget* format;		/* format button */
  GtkWidget* mount_label;	/* label in the mount button */
  
  struct mountinfo* next;	/* linked list... */
};

/* GUI building stuff */
void create_usermount_window();
GtkWidget* create_mount_table(struct mountinfo* list);

/* "callbacks" */
void mount_button(GtkWidget* widget, struct mountinfo* info);
void format_button(GtkWidget* widget, struct mountinfo* info);

/* utility stuff... */
struct mountinfo* build_mountinfo_list();
void normalize_mountinfo_list(struct mountinfo* list, struct mountinfo* current);
int user_mount_toggle(char* file, int bool);
void user_format(char* device, char* fstype, int lowlevel);

int
main(int argc, char* argv[])
{
  /* first set up our locale info for gettext. */
  setlocale(LC_ALL, "");
  bindtextdomain("userhelper", "/usr/share/locale");
  textdomain("userhelper");

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
  GtkWidget* dialog;
  GtkWidget* cancel;
  GtkWidget* mount_table;
  struct mountinfo* info;
  
  dialog = gtk_dialog_new();
  gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width(GTK_CONTAINER(dialog), 5);
  gtk_window_set_title(GTK_WINDOW(dialog), i18n("User Mount Tool"));
  gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
		     (GtkSignalFunc) gtk_main_quit, NULL);
  gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
		     (GtkSignalFunc) gtk_main_quit, NULL);

  cancel = gtk_button_new_with_label(UD_EXIT_TEXT);
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(cancel)->child), 4, 0);
  gtk_signal_connect(GTK_OBJECT(cancel), "clicked", 
		     (GtkSignalFunc) gtk_main_quit, NULL);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), 
		     cancel, TRUE, TRUE, 4);

  info = build_mountinfo_list();
  if(info == NULL)
    {
      mount_table = gtk_label_new(i18n("There are no user mountable filesystems.\nContact your administrator."));
      gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog)->vbox), 5);
    }
  else
    {
      mount_table = create_mount_table(info);
    }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), mount_table,
		     TRUE, TRUE, 0);

  GTK_WIDGET_SET_FLAGS(cancel, GTK_CAN_DEFAULT);
  gtk_widget_grab_default(cancel);

  gtk_widget_show(mount_table);
  gtk_widget_show(cancel);
  gtk_widget_show(dialog);
}

GtkWidget*
create_mount_table(struct mountinfo* list)
{
  GtkWidget* mount_table;
  GtkWidget* mountpoint;
  GtkWidget* device;
  GtkWidget* fstype;
  GtkWidget* mount;
  GtkWidget* mount_label;
  GtkWidget* format;
  GtkWidget* headings;
  GtkWidget* sep;
  struct mountinfo* current;
  int row;

  row = 0;
  
  mount_table = gtk_table_new(MAXFSTAB, 5, FALSE);
  gtk_container_border_width(GTK_CONTAINER(mount_table), 5);
  gtk_table_set_row_spacings(GTK_TABLE(mount_table), 5);
  gtk_table_set_col_spacings(GTK_TABLE(mount_table), 5);

  headings = gtk_label_new(i18n("Directory"));
  gtk_table_attach(GTK_TABLE(mount_table), headings,
		   0, 1, row, row+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(headings);

  headings = gtk_label_new(i18n("Device"));
  gtk_table_attach(GTK_TABLE(mount_table), headings,
		   1, 2, row, row+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(headings);

  headings = gtk_label_new(i18n("Type"));
  gtk_table_attach(GTK_TABLE(mount_table), headings,
		   2, 3, row, row+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(headings);

  row++;

  sep = gtk_hseparator_new();
  gtk_table_attach(GTK_TABLE(mount_table), sep,
		   0, 5, row, row+1,
		   GTK_EXPAND | GTK_FILL, 
		   GTK_EXPAND | GTK_FILL, 
		   0, 0);
  gtk_widget_show(sep);

  row++;

  current = list;

  normalize_mountinfo_list(list, NULL);

  while(current != NULL)
    {
      mountpoint = gtk_label_new(current->mi_dir);
      gtk_table_attach(GTK_TABLE(mount_table), mountpoint, 
		       0, 1, row, row+1,
		       GTK_EXPAND | GTK_FILL, 
		       GTK_EXPAND | GTK_FILL, 
		       0, 0);
      gtk_widget_show(mountpoint);

      device = gtk_label_new(current->mi_dev);
      gtk_table_attach(GTK_TABLE(mount_table), device, 
		       1, 2, row, row+1,
		       GTK_EXPAND | GTK_FILL, 
		       GTK_EXPAND | GTK_FILL, 
		       0, 0);
      gtk_widget_show(device);
	  
      fstype = gtk_label_new(current->mi_fstype);
      gtk_table_attach(GTK_TABLE(mount_table), fstype, 
		       2, 3, row, row+1,
		       GTK_EXPAND | GTK_FILL, 
		       GTK_EXPAND | GTK_FILL, 
		       0, 0);
      gtk_widget_show(fstype);

      mount = gtk_button_new();
      gtk_widget_set_usize(mount, 60, 0);
      mount_label = gtk_label_new(i18n(UMOUNT_TEXT));
      gtk_misc_set_padding(GTK_MISC(mount_label), 4, 0);
      gtk_container_add(GTK_CONTAINER(mount), mount_label);
      gtk_widget_show(mount_label);

      current->mount = mount;
      current->mount_label = mount_label;

      if(current->mi_status)
	gtk_label_set(GTK_LABEL(current->mount_label), i18n(UMOUNT_TEXT));
      else
	gtk_label_set(GTK_LABEL(current->mount_label), i18n(MOUNT_TEXT));

      gtk_signal_connect(GTK_OBJECT(mount), "clicked",
			 (GtkSignalFunc) mount_button,
			 current);
      gtk_table_attach(GTK_TABLE(mount_table), mount,
		       3, 4, row, row+1,
		       GTK_EXPAND | GTK_FILL,
		       GTK_EXPAND | GTK_FILL,
		       0, 0);
      gtk_widget_show(mount);

      format = gtk_button_new_with_label(i18n("Format"));
      gtk_misc_set_padding(GTK_MISC(GTK_BIN(format)->child), 4, 0);
      gtk_widget_set_usize(format, 60, 0);
      current->format = format;
      gtk_widget_set_sensitive(format, current->mi_writable);
      if(current->mi_status)
	{
	  gtk_label_set(GTK_LABEL(current->mount_label), i18n(UMOUNT_TEXT));
 	  gtk_widget_set_sensitive(format, FALSE);
	  normalize_mountinfo_list(NULL, current);
	}
      /* this is a kludge, I don't want to be able to format CD-ROM
       * devices, even if they are writable... which is often the case
       * for users so they can play CDs.  Blech.  I can't think of a
       * better way to determine if it's a CD or now.  Sorry for
       * anyone using an writable iso9660 filesystem to master
       * CDs... they'll just have to do it by hand, like they have been
       * for so long.
       * It might be reasonable to check for the 'ro' option in the
       * fstab, but I don't that's a very good alternative.
       */
      if(strcmp(current->mi_fstype, "iso9660") == 0)
	{
	  gtk_widget_set_sensitive(format, FALSE);
	}
      
      gtk_signal_connect(GTK_OBJECT(format), "clicked", 
			 (GtkSignalFunc) format_button, 
			 current);
      gtk_table_attach(GTK_TABLE(mount_table), format, 
		       4, 5, row, row+1,
		       GTK_EXPAND | GTK_FILL, 
		       GTK_EXPAND | GTK_FILL, 
		       0, 0);
      gtk_widget_show(format);
 
      current = current->next;
      row++;
    }

  return mount_table;
}

/* consider ridding myself of these "callbacks" */
void
mount_button(GtkWidget* widget, struct mountinfo* info)
{
  int sensitive;

  if(user_mount_toggle(info->mi_dir, info->mi_status))
    {
      info->mi_status = !info->mi_status;
      if(info->mi_status)
	{
	  sensitive = FALSE;
	  gtk_label_set(GTK_LABEL(info->mount_label), i18n(UMOUNT_TEXT));
	}
      else
	{
	  sensitive = TRUE;
	  gtk_label_set(GTK_LABEL(info->mount_label), i18n(MOUNT_TEXT));
	}
      gtk_widget_set_sensitive(info->format, sensitive && info->mi_writable);
      normalize_mountinfo_list(NULL, info);
    }

}

void
format_confirm_button(struct mountinfo* info)
{
  user_format(info->mi_dev, info->mi_fstype, info->mi_fdformat);
}

void
fdformat_check_button(GtkWidget* check, struct mountinfo* info)
{
  info->mi_fdformat = GTK_TOGGLE_BUTTON(check)->active;
}

void
format_button(GtkWidget* widget, struct mountinfo* info)
{
  GtkWidget* dialog;
  GtkWidget* label;
  GtkWidget* confirm_button;
  GtkWidget* cancel_button;
  GtkWidget* fdformat_check;

  dialog = gtk_dialog_new();
  gtk_window_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_title(GTK_WINDOW(dialog), "Confirm");
  gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), 5);
  
  label = gtk_label_new(i18n("Are you sure?\nYou will destroy any data on that disk.\n"));

  confirm_button = gtk_button_new_with_label(i18n("Yes"));
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(confirm_button)->child), 4, 0);
  gtk_widget_set_usize(confirm_button, 50, 0);
  gtk_signal_connect_object(GTK_OBJECT(confirm_button), "clicked",
			    (GtkSignalFunc) format_confirm_button,
			    (gpointer) info);
  gtk_signal_connect_object(GTK_OBJECT(confirm_button), "clicked",
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) dialog);

  cancel_button = gtk_button_new_with_label(i18n("No"));
  gtk_misc_set_padding(GTK_MISC(GTK_BIN(cancel_button)->child), 4, 0);
  gtk_widget_set_usize(cancel_button, 50, 0);
  gtk_signal_connect_object(GTK_OBJECT(cancel_button), "clicked",
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) dialog);

  fdformat_check = gtk_check_button_new_with_label(i18n("Do low level format."));
  gtk_signal_connect(GTK_OBJECT(fdformat_check), "toggled",
		     (GtkSignalFunc) fdformat_check_button,
		     info);
  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(fdformat_check), TRUE);  

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
		     fdformat_check,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), 
		     confirm_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->action_area), 
		     cancel_button, FALSE, FALSE, 0);

  gtk_widget_show(confirm_button);
  gtk_widget_show(cancel_button);
  gtk_widget_show(label);
  gtk_widget_show(fdformat_check);
  gtk_widget_show(dialog);

}

struct mountinfo*
build_mountinfo_list()
{
  /* parses /etc/fstab using getmntent, and other tricks to filll in
   * the info for all filesystems with the 'user' option... unless, of
   * course getuid() == 0, then get 'em all.  No reason root shouldn't
   * be allowed to use this.
   */
  struct mountinfo* retval;
  struct mountinfo* tmp;
  struct stat stat_buf_1;
  struct stat stat_buf_2;
  char* parent_dir;
  struct mntent* fstab_entry;
  FILE* fstab;
  uid_t euid;

  fstab = setmntent(MNTTAB, "r");
  retval = NULL;

  euid = geteuid();
 
  while((fstab_entry = getmntent(fstab)) != NULL)
    {
      struct stat sb;
      int owner = 0;

      if (!strncmp(fstab_entry->mnt_fsname, "/dev/", 5))
        {
	  stat(fstab_entry->mnt_fsname, &sb);
	  if (sb.st_uid == getuid()) owner=1;
	}

      if((hasmntopt(fstab_entry, "user") != NULL ||
	 (hasmntopt(fstab_entry, "owner") != NULL && owner) ||
	 euid == 0)
	 && strcmp(fstab_entry->mnt_type, "swap") != 0)
	{
	  tmp = g_malloc(sizeof(struct mountinfo));
	  tmp->next = retval;
	  retval = tmp;

	  retval->mi_dir = strdup(fstab_entry->mnt_dir);
	  retval->mi_dev = strdup(fstab_entry->mnt_fsname);
	  retval->mi_fstype = strdup(fstab_entry->mnt_type);

	  /* FIXME: aggressive error checking. */
	  stat(retval->mi_dir, &stat_buf_1);
	  parent_dir = g_strdup(retval->mi_dir);
	  strrchr(parent_dir, '/')[0] = '\0';
	  stat(parent_dir, &stat_buf_2);
	  if(stat_buf_1.st_dev != stat_buf_2.st_dev)
	    {
	      retval->mi_status = TRUE;
	    }
	  else
	    {
	      retval->mi_status = FALSE;
	    }
	  g_free(parent_dir);

	  if(access(retval->mi_dev, W_OK) == 0 || euid == 0)
	    {
	      retval->mi_writable = TRUE;
	    }
	  else
	    {
	      retval->mi_writable = FALSE;
	    }

	  retval->mi_fdformat = TRUE;
	  /* buttons are allocated in create_mount_table() */
	}
    }

  return retval;
}

/* this is a really bad name.
 * Basically, this function goes through the list and makes sure all
 * the appropriate format and mount buttons are turned off, if they're
 * not possible... i.e., if I have /dev/fd0 mounted on /mnt/floppy, I
 * can't mount it on /mnt/floppy-dos... so the mount button should be
 * greyed out.  The same goes for the format buttons... 
 * If list is NULL, then the static internal version is used as a
 * pointer to the head of the list.  Otherwise the internal version is
 * set to the passed value.
 * current is the current item, so I cam be sure not to turn off the
 * mount button for that one... otherwise, it would be bad. :)
 */
void
normalize_mountinfo_list(struct mountinfo* list, struct mountinfo* current)
{
  static struct mountinfo* head;
  struct mountinfo* tmp;

  if(list != NULL)
    {
      head = list;
    }

  if(current == NULL)
    {
      return;
    }

  tmp = head;
  if(current->mi_status)
    {
      while(tmp != NULL)
	{
	  if(strcmp(tmp->mi_dev, current->mi_dev) == 0 &&
	     strcmp(tmp->mi_dir, current->mi_dir) != 0 &&
	     strcmp(tmp->mi_dev, "none") != 0)
	    {
	      gtk_widget_set_sensitive(tmp->mount, FALSE);
	      gtk_widget_set_sensitive(tmp->format, FALSE);
	    }
	  tmp = tmp->next;
	}
    }
  else
    {
    while(tmp != NULL)
	{
	  if(strcmp(tmp->mi_dev, current->mi_dev) == 0)
	    {
	      gtk_widget_set_sensitive(tmp->mount, TRUE);
	      if(tmp->mi_writable)
		{
		  gtk_widget_set_sensitive(tmp->format, TRUE);
		}
	    }
	  else if(strcmp(tmp->mi_fstype, "iso9660") == 0)
	    {
	      /* still a kludge... see above, where I do the same
	       * thing in create_mount_table.
	       */
	      gtk_widget_set_sensitive(tmp->format, FALSE);
	    }
	  tmp = tmp->next;
	}
    }
     
}

int
user_mount_toggle(char* file, int bool)
{
  /* more aggressive error checking. */

  GtkWidget* message_box;

  char* cmd;
  int childout[2];
  int childerr[2];
  pid_t pid;

  char outline[MAXLINE];
  char errline[MAXLINE];
  int n;
  int count;
  int childstatus;

  if(!bool)
    {
      cmd = "/bin/mount";
    }
  else
    {
      cmd = "/bin/umount";
    }
  
  if(pipe(childout) < 0 || pipe(childerr) < 0)
    {
      fprintf(stderr, i18n("Pipe error.\n"));
      exit(1);
    }

  if((pid = fork()) < 0)
    {
      fprintf(stderr, i18n("Cannot fork().\n"));
    }
  else if(pid > 0)		/* parent */
    {
      close(childout[1]);
      close(childerr[1]);

      waitpid(pid, &childstatus, 0);

      if(!WIFEXITED(childstatus) || WEXITSTATUS(childstatus) != 0)
	{
	  n = 0;

	  count = read(childerr[0], errline, MAXLINE);
	  if(count > 0)
	    {
	      errline[count] = '\0';
	      message_box = create_error_box(errline, NULL);
	      gtk_widget_show(message_box);
	    }
	  count = read(childout[0], outline, MAXLINE);
	  if(count > 0)
	    {
	      outline[count] = '\0';
	      message_box = create_error_box(outline, NULL);
	      gtk_widget_show(message_box);
	    }
	  return FALSE;
	}

    }
  else				/* child */
    {
      close(childout[0]);
      close(childerr[0]);

      if(childout[1] != STDOUT_FILENO)
	{
	  if(dup2(childout[1], STDOUT_FILENO) != STDOUT_FILENO)
	    {
	      fprintf(stdout, i18n("dup2() error.\n"));
	      exit(2);
	    }
	}
      if(childerr[1] != STDERR_FILENO)
	{
	  if(dup2(childerr[1], STDERR_FILENO) != STDERR_FILENO)
	    {
	      fprintf(stdout, i18n("dup2() error.\n"));
	      exit(2);
	    }
	}

      if(execl(cmd, cmd, file, 0) < 0)
	{
	  fprintf(stderr, i18n("execl() error, errno=%d\n"), errno);
	}

      _exit(0);

    }

  return TRUE;

}

void
user_format(char* device, char* fstype, int lowlevel)
{

  /* may want to have a return value on user_format, so I can check
   * success... not really a big deal, but I can do it just to be
   * consistant with user_mount_toggle.
   */

  char* mkfs_cmd = "/sbin/mkfs";
  char* mkfs_fstype = "-t";
  char* fdformat_cmd = "/usr/bin/fdformat";
  int childout[2];
  int childerr[2];
  char errline[MAXLINE];
  char outline[MAXLINE];
  pid_t pid;
  int childstatus;
  int count, n;
  GtkWidget* message_box;

  if(pipe(childout) < 0 || pipe(childerr) < 0)
    {
      fprintf(stderr, i18n("Pipe error.\n"));
      exit(1);
    }

  /* fdformat */
  if(!lowlevel)
    {
      /* noop, basically */
    }
  else if((pid = fork()) < 0)
    {
      fprintf(stderr, i18n("Cannot fork().\n"));
    }
  else if(pid > 0)
    {
      close(childout[1]);
      close(childerr[1]);

      /* If I end up hacking in some kind of progress bar, this will
       * be tricky... I'll have to give the option to not hang, i.e.,
       * if it exits right away, something went wrong... if it doesn't
       * exit right away it's okay for now... 'twill be tricky.
       * No it won't... I'll just have to switch to gdk_input and a
       * SIGCHLD handler rather than select and waitpid().  In fact, I
       * should probably do that anyway, so the window will refresh
       * while I'm waiting for the format... 
       */
      waitpid(pid, &childstatus, 0);

      if(!WIFEXITED(childstatus) || WEXITSTATUS(childstatus) != 0)
	{
	  n = 0;
	  
	  count = read(childerr[0], errline, MAXLINE);
	  if(count > 0)
	    {
	      errline[count] = '\0';
	      message_box = create_error_box(errline, NULL);
	      gtk_widget_show(message_box);
	    }
	  count = read(childout[0], outline, MAXLINE);
	  if(count > 0)
	    {
	      outline[count] = '\0';
	      message_box = create_message_box(outline, NULL);
	      gtk_widget_show(message_box);
	    }

	  /* if this failed, mkfs will fail.  I really wish I could do
	   * more understandable error messages. 
	   */
	  return;
	}
    }
  else
    {
      close(childout[0]);
      close(childerr[0]);

      if(childout[1] != STDOUT_FILENO)
	{
	  if(dup2(childout[1], STDOUT_FILENO) != STDOUT_FILENO)
	    {
	      fprintf(stdout, i18n("dup2() error.\n"));
	      exit(2);
	    }
	}
      if(childerr[1] != STDERR_FILENO)
	{
	  if(dup2(childerr[1], STDERR_FILENO) != STDERR_FILENO)
	    {
	      fprintf(stdout, i18n("dup2() error.\n"));
	      exit(2);
	    }
	}

      if(execl(fdformat_cmd, fdformat_cmd, device, 0) < 0)
	{
	  fprintf(stderr, i18n("execl() error, errno=%d\n"), errno);
	}

      _exit(0);

    }

  /* mkfs */
  if(pipe(childout) < 0 || pipe(childerr) < 0)
    {
      fprintf(stderr, i18n("Pipe error.\n"));
      exit(1);
    }

  if((pid = fork()) < 0)
    {
      fprintf(stderr, i18n("Cannot fork().\n"));
    }
  else if(pid > 0)
    {
      close(childout[1]);
      close(childerr[1]);

      /* waitpid() to get the exitvalue */
      /* FIXME: aggressive error checking */
      waitpid(pid, &childstatus, 0);

      if(WEXITSTATUS(childstatus) != 0)
	{
	  n = 0;
	  count = read(childerr[0], errline, MAXLINE);
	  if(count > 0)
	    {
	      errline[count] = '\0';
	      message_box = create_error_box(errline, NULL);
	      gtk_widget_show(message_box);
	    }
	  count = read(childout[0], outline, MAXLINE);
	  if(count > 0)
	    {
	      outline[count] = '\0';
	      message_box = create_message_box(outline, NULL);
	      gtk_widget_show(message_box);
	    }
	}
    }
  else
    {
      close(childout[0]);
      close(childerr[0]);

      if(childout[1] != STDOUT_FILENO)
	{
	  if(dup2(childout[1], STDOUT_FILENO) != STDOUT_FILENO)
	    {
	      fprintf(stdout, i18n("dup2() error.\n"));
	      exit(2);
	    }
	}
      if(childerr[1] != STDERR_FILENO)
	{
	  if(dup2(childerr[1], STDERR_FILENO) != STDERR_FILENO)
	    {
	      fprintf(stdout, i18n("dup2() error.\n"));
	      exit(2);
	    }
	}

      if(execl(mkfs_cmd, mkfs_cmd, mkfs_fstype, fstype, device, 0) < 0)
	{
	  fprintf(stderr, i18n("execl() error, errno=%d\n"), errno);
	}

      _exit(0);

    }

}
