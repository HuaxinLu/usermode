#include <stdio.h>
#include <gtk/gtk.h>
#include "userdialogs.h"

void hello_world(char* str);

int
main(int argc, char* argv[])
{
  GtkWidget* msg;

  gtk_init(&argc, &argv);

  msg = create_message_box("Hello world!", "Hello", hello_world,
			   (gpointer) "Otto");

  gtk_signal_connect_object(GTK_OBJECT(msg), "destroy", 
			    (GtkSignalFunc) gtk_main_quit, NULL);
  gtk_signal_connect_object(GTK_OBJECT(msg), "destroy", 
			    (GtkSignalFunc) hello_world, "otto");
  
  gtk_main();

  return 0;
}

void
hello_world(char* str)
{
  printf("Hello world, %s.\n", str);
}
