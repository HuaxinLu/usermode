#include <stdio.h>
#include <gtk/gtk.h>
#include "userdialogs.h"

void hello_world();
void hello_world2(GtkWidget* button, GtkWidget* entry);

int
main(int argc, char* argv[])
{
  GtkWidget* msg;

  gtk_init(&argc, &argv);

  msg = create_message_box("Hello world!\nLet's make this a really big message box.", "Hello");

  gtk_signal_connect_object(GTK_OBJECT(msg), "destroy", 
			    (GtkSignalFunc) gtk_main_quit, NULL);
  gtk_signal_connect_object(GTK_OBJECT(msg), "destroy", 
			    (GtkSignalFunc) hello_world, "otto");

  msg = create_query_box("Hello world!", "Hi!", hello_world2);

  msg = create_error_box("ERROR!\nLet's make this a really big message box.", NULL);

  gtk_main();

  return 0;
}

void
hello_world(char* str)
{
  printf("Hello world, %s.\n", str);
}

void
hello_world2(GtkWidget* button, GtkWidget* entry)
{
  printf("Hello world, %s.\n", gtk_entry_get_text(GTK_ENTRY(entry)));
}
