#include <stdio.h>
#include <glade/glade.h>
#include <glade/glade-xml.h>
#include <gtk/gtk.h>
#include "userdialogs.h"

void
hello_world(GtkWidget *ignored, gpointer data)
{
	printf("Hello world, %s.\n", (char*) data);
}

void
hello_world2(GtkWidget *button, gpointer entry)
{
	printf("Hello world, %s.\n", gtk_entry_get_text(GTK_ENTRY(entry)));
}

int
main(int argc, char *argv[])
{
	GtkWidget *msg;

	bindtextdomain("usermode", "/usr/share/locale");
	textdomain("usermode");

	gtk_set_locale();
	gtk_init(&argc, &argv);
	glade_init();

	msg =
	    create_message_box
	    ("Hello world!\nLet's make this a really big message box.",
	     "Hello");

	gtk_signal_connect_object(GTK_OBJECT(msg), "destroy",
				  (GtkSignalFunc) gtk_main_quit, NULL);
	gtk_signal_connect(GTK_OBJECT(msg), "destroy",
			   (GtkSignalFunc) hello_world,
			   (gpointer)"otto");

	msg = create_query_box("Hello world!", "Hi!",
			       (GtkSignalFunc)hello_world2);

	msg = create_invisible_query_box("Hello world!", "Hi!",
					 (GtkSignalFunc)hello_world2);

	msg = create_error_box("ERROR!\nLet's make this a really big message box.", NULL);

	gtk_main();

	return 0;
}
