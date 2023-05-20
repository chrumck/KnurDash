#define G_LOG_USE_STRUCTURED
#include <gtk/gtk.h>

#include "dataContracts.h"
#include "brightness.c"
#include "sensors.c"

static void shutDown(GtkWidget* widget, gpointer data)
{
    gtk_widget_set_sensitive(widget, FALSE);
    gtk_button_set_label(GTK_BUTTON(widget), "Turning Off...");

    WorkerData* workerData = (WorkerData*)data;
    workerData->isShuttingDown = TRUE;

    while (workerData->isSensorWorkerRunning == TRUE) {
        g_usleep(100000);
        while (gtk_events_pending()) gtk_main_iteration();
    }

#ifdef NDEBUG
    system("sudo shutdown now");
#else
    gtk_main_quit();
#endif
}

int main(int argc, char* argv[])
{
    gtk_init(&argc, &argv);


    char exeFilePath[PATH_MAX + 5];
    readlink("/proc/self/exe", exeFilePath, PATH_MAX);

    char cssFilePath[PATH_MAX + NAME_MAX];
    strcpy(cssFilePath, exeFilePath);
    strcat(cssFilePath, ".css");

    GError* error = NULL;

    GtkCssProvider* cssProvider = gtk_css_provider_new();
    if (gtk_css_provider_load_from_path(cssProvider, cssFilePath, &error) == 0)
    {
        g_printerr("Error loading file: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    GdkScreen* screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    char uiFilePath[PATH_MAX + NAME_MAX];
    strcpy(uiFilePath, exeFilePath);
    strcat(uiFilePath, ".ui");

    GtkBuilder* builder = gtk_builder_new();
    if (gtk_builder_add_from_file(builder, uiFilePath, &error) == 0)
    {
        g_printerr("Error loading file: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    GObject* window = gtk_builder_get_object(builder, "window");

    gtk_window_fullscreen(GTK_WINDOW(window));

    WorkerData workerData = { .builder = builder, .isShuttingDown = FALSE, .isSensorWorkerRunning = FALSE };
    GThread* sensorsWorker = g_thread_new("readAnalogSensors", sensorWorkerLoop, &workerData);

    g_signal_connect(window, "destroy", G_CALLBACK(shutDown), &workerData);

    GObject* button = gtk_builder_get_object(builder, "brightnessDown");
    g_signal_connect(button, "clicked", G_CALLBACK(setBrightnessDown), NULL);

    button = gtk_builder_get_object(builder, "brightnessUp");
    g_signal_connect(button, "clicked", G_CALLBACK(setBrightnessUp), NULL);

    button = gtk_builder_get_object(builder, "turnOff");
    g_signal_connect(button, "clicked", G_CALLBACK(shutDown), &workerData);

    gtk_main();

    return 0;
}