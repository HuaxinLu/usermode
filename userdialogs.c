/*
 * Copyright (C) 1997,2001 Red Hat, Inc.
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
 *
 */

#include "config.h"
#include <locale.h>
#include <libintl.h>
#include <glade/glade-xml.h>
#include <gtk/gtk.h>
#include "userdialogs.h"

#define DIALOG_XML_NAME "userdialog-xml"

#if GTK_CHECK_VERSION(1,3,10)
GtkWidget *
create_message_box(gchar *message, gchar *title)
{
	GtkWidget *dialog;
	dialog =  gtk_message_dialog_new(NULL, 0,
					 GTK_MESSAGE_INFO,
					 GTK_BUTTONS_CLOSE,
					 "%s", message);
	if(title) {
		gtk_window_set_title(GTK_WINDOW(dialog), title);
	}
	g_signal_connect_object(G_OBJECT(dialog), "response",
				(GtkSignalFunc)gtk_widget_destroy, dialog, 0);
	gtk_widget_show_all(dialog);
	return dialog;
}

GtkWidget *
create_error_box(gchar * error, gchar * title)
{
	GtkWidget *dialog;
	dialog =  gtk_message_dialog_new(NULL, 0,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE,
					 "%s", error);
	if(title) {
		gtk_window_set_title(GTK_WINDOW(dialog), title);
	}
	g_signal_connect_object(G_OBJECT(dialog), "response",
				(GtkSignalFunc)gtk_widget_destroy, dialog, 0);
	gtk_widget_show_all(dialog);
	return dialog;
}
#else
GtkWidget *
create_message_box(gchar *message, gchar *title)
{
	GladeXML *xml;
	GtkWidget *window, *label, *button;

	xml = glade_xml_new(DATADIR "/" PACKAGE "/" PACKAGE ".glade",
			    "message_box", PACKAGE);

	window = glade_xml_get_widget(xml, "message_box");
	gtk_object_set_data(GTK_OBJECT(window), DIALOG_XML_NAME, xml);

	label = glade_xml_get_widget(xml, "message");
	button = glade_xml_get_widget(xml, "close");

	if(title) {
		gtk_window_set_title(GTK_WINDOW(window), title);
	}
	gtk_label_set_text(GTK_LABEL(label), message);

	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				  (GtkSignalFunc) gtk_widget_destroy,
				  (gpointer) window);

	return window;
}

GtkWidget *
create_error_box(gchar * error, gchar * title)
{
	return create_message_box(error, title);
}
#endif

#if GTK_CHECK_VERSION(1,3,10)
static void
relay_value(GtkWidget *dialog, GtkResponseType response, GtkSignalFunc func)
{
	void (*callback)(GtkWidget *dialog, GtkWidget *entry) = NULL;
	callback = (void(*)(GtkWidget *dialog, GtkWidget *entry)) func;
	if(response == GTK_RESPONSE_OK) {
		GtkWidget *entry;
		entry = g_object_get_data(G_OBJECT(dialog), "entry");
		if(GTK_IS_WIDGET(entry)) {
			callback(dialog, entry);
		}
	}
}

static GtkWidget *
create_query_box_i(gchar * prompt, gchar * title, GtkSignalFunc func,
		   gboolean visible)
{
	GtkWidget *dialog, *box, *entry;
	dialog = gtk_message_dialog_new(NULL, 0,
					GTK_MESSAGE_QUESTION,
					GTK_BUTTONS_OK_CANCEL,
					"%s", prompt);
	if(title) {
		gtk_window_set_title(GTK_WINDOW(dialog), title);
	}
	box = (GTK_DIALOG(dialog))->vbox;

	entry = gtk_entry_new();
	g_object_set_data(G_OBJECT(dialog), "entry", entry);
	gtk_entry_set_visibility(GTK_ENTRY(entry), visible);

	gtk_box_pack_start_defaults(GTK_BOX(box), entry);

	g_signal_connect(G_OBJECT(dialog), "response",
			 (GtkSignalFunc)relay_value, func);
	g_signal_connect_object(G_OBJECT(dialog), "response",
				(GtkSignalFunc)gtk_widget_destroy, NULL,
				G_CONNECT_AFTER);

	gtk_widget_show_all(dialog);

	return dialog;
}
#else
static GtkWidget *
create_query_box_i(gchar * prompt, gchar * title, GtkSignalFunc func,
		   gboolean visible)
{
	GladeXML *xml;
	GtkWidget *window, *label, *entry, *ok, *cancel;

	xml = glade_xml_new(DATADIR "/" PACKAGE "/" PACKAGE ".glade",
			    "query_box", PACKAGE);

	window = glade_xml_get_widget(xml, "query_box");
	gtk_object_set_data(GTK_OBJECT(window), DIALOG_XML_NAME, xml);

	label = glade_xml_get_widget(xml, "question");
	entry = glade_xml_get_widget(xml, "entry");
	ok = glade_xml_get_widget(xml, "ok");
	cancel = glade_xml_get_widget(xml, "cancel");

	if(title) {
		gtk_window_set_title(GTK_WINDOW(window), title);
	}
	gtk_label_set_text(GTK_LABEL(label), prompt);
	gtk_entry_set_visibility(GTK_ENTRY(entry), visible);

	gtk_signal_connect_object(GTK_OBJECT(ok), "clicked",
				  (GtkSignalFunc) gtk_widget_hide,
				  (gpointer) window);
	gtk_signal_connect(GTK_OBJECT(ok), "clicked",
			   (GtkSignalFunc) func, (gpointer) entry);

	gtk_signal_connect_object(GTK_OBJECT(cancel), "clicked",
				  (GtkSignalFunc) gtk_widget_destroy,
				  (gpointer) window);

	/* FIXME: close the dialog when the cancel button is clicked, and
	 * attach a signal handler to the entry field */

	return window;
}
#endif

GtkWidget *
create_invisible_query_box(gchar * prompt, gchar * title,
			   GtkSignalFunc func)
{
	return create_query_box_i(prompt, title, func, FALSE);
}

GtkWidget *
create_query_box(gchar * prompt, gchar * title, GtkSignalFunc func)
{
	return create_query_box_i(prompt, title, func, TRUE);
}
