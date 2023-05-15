#include <gtk/gtk.h>

static void print_hello(GtkWidget* widget, gpointer   data)
{
    g_print("Hello World\n");
}

int main(int argc, char* argv[])
{
    gtk_init(&argc, &argv);

    GError* error = NULL;
    GtkBuilder* builder = gtk_builder_new();
    char dirNameBuffer[PATH_MAX + 1];
    const char* linkName = "/proc/self/exe";
    const int ret = readlink(linkName, dirNameBuffer, PATH_MAX);

    printf(dirNameBuffer);

    if (gtk_builder_add_from_file(builder, "./main.ui", &error) == 0)
    {
        g_printerr("Error loading file: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    /* Connect signal handlers to the constructed widgets. */
    GObject* window = gtk_builder_get_object(builder, "window");
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GObject* button = gtk_builder_get_object(builder, "button1");
    g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);

    button = gtk_builder_get_object(builder, "button2");
    g_signal_connect(button, "clicked", G_CALLBACK(print_hello), NULL);

    button = gtk_builder_get_object(builder, "quit");
    g_signal_connect(button, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    gtk_main();

    return 0;
}