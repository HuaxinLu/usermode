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

#include "userdialogs.h"

GtkWidget*
create_message_box(gchar* message, gchar* title, 
		   GtkSignalFunc func, gpointer arg)
{
  GtkWidget* message_box;
  GtkWidget* label;
  GtkWidget* ok;

  message_box = gtk_dialog_new();
  if(title == NULL)
    gtk_window_set_title(GTK_WINDOW(message_box), "Message");
  else
    gtk_window_set_title(GTK_WINDOW(message_box), title);

  label = gtk_label_new(message);
  ok = gtk_button_new_with_label(UD_OK_TEXT);
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", 
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) message_box);
  if(func != NULL)
    {
      /* what to do, if I don't want to pass an arg? */
      /* really should use varargs... simpliest solution, 'cept there
       * are no signal_connect functions that'll let me pass multiple
       * arg.. which would be -really- nice. 
       */
      gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", func, arg);
    }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(message_box)->vbox), label,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(message_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(message_box);

  return message_box;

}

GtkWidget*
create_error_box(gchar* error, gchar* title, 
		 GtkSignalFunc func, gpointer arg)
{
  GtkWidget* error_box;
  GtkWidget* label;
  GtkWidget* ok;

  error_box = gtk_dialog_new();
  if(title == NULL)
    gtk_window_set_title(GTK_WINDOW(error_box), "Error");
  else
    gtk_window_set_title(GTK_WINDOW(error_box), title);

  label = gtk_label_new(error);
  ok = gtk_button_new_with_label(UD_OK_TEXT);
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", 
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) error_box);
  if(func != NULL)
    {
      gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", func, arg);
    }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(error_box)->vbox), label,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(error_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(error_box);

  return error_box;
}

GtkWidget*
create_query_box(gchar* prompt, gchar* title, 
		 GtkSignalFunc func, gpointer arg)
{
  GtkWidget* query_box;
  GtkWidget* label;
  GtkWidget* entry;
  GtkWidget* ok;

  query_box = gtk_dialog_new();
  if(title == NULL)
    gtk_window_set_title(GTK_WINDOW(query_box), "Prompt");
  else
    gtk_window_set_title(GTK_WINDOW(query_box), "Prompt");
  

  label = gtk_label_new(prompt);
  entry = gtk_entry_new();
  ok = gtk_button_new_with_label(UD_OK_TEXT);
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", 
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) query_box);
  if(func != NULL)
    {
      gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", func, arg);
    }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), label,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), entry,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(entry);
  gtk_widget_show(query_box);

  return query_box;
}

GtkWidget*
create_invisible_query_box(gchar* prompt, gchar* title, 
			   GtkSignalFunc func, gpointer arg)
{
  GtkWidget* query_box;
  GtkWidget* label;
  GtkWidget* entry;
  GtkWidget* ok;
  
  query_box = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(query_box), "Prompt");

  label = gtk_label_new(prompt);
  entry = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
/*   gtk_signal_connect(GTK_OBJECT(entry), "destroy", */
/* 		     (GtkSignalFunc) user_input, (gpointer)fd); */
  ok = gtk_button_new_with_label("OK");
  gtk_signal_connect_object(GTK_OBJECT(ok), "clicked",
			    (GtkSignalFunc) gtk_widget_destroy,
			    (gpointer) query_box);
  if(func != NULL)
    {
      gtk_signal_connect_object(GTK_OBJECT(ok), "clicked", func, arg);
    }

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), label,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->vbox), entry,
		     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(query_box)->action_area), ok,
		     FALSE, FALSE, 0);
  
  gtk_widget_show(ok);
  gtk_widget_show(label);
  gtk_widget_show(entry);
  gtk_widget_show(query_box);

  return query_box;
}
