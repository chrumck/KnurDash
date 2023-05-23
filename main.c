#define G_LOG_USE_STRUCTURED
#include <gtk/gtk.h>

#include "dataContracts.h"
#include "ui.c"
#include "sensors.c"

#define FILE_PATH_LENGTH (PATH_MAX + 1)

int main(int argc, char* argv[])
{
    gtk_init(&argc, &argv);

    static char exeFilePath[FILE_PATH_LENGTH];
    readlink("/proc/self/exe", exeFilePath, FILE_PATH_LENGTH);

    static char cssFilePath[FILE_PATH_LENGTH];
    sprintf(cssFilePath, "%s%s", exeFilePath, ".css");

    GError* error = NULL;
    GtkCssProvider* cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(cssProvider, cssFilePath, &error);

    if (error != NULL) g_error("Error loading file: %s\n", error->message);

    GdkScreen* screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    static char uiFilePath[FILE_PATH_LENGTH];
    sprintf(uiFilePath, "%s%s", exeFilePath, ".ui");

    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, uiFilePath, &error);
    if (error != NULL) g_error("Error loading file: %s\n", error->message);

    GObject* window = gtk_builder_get_object(builder, "window");

    gtk_window_fullscreen(GTK_WINDOW(window));

    WorkerData workerData = { .builder = builder, };
    GThread* sensorsWorker = g_thread_new("readAnalogSensors", sensorWorkerLoop, &workerData);

    g_signal_connect(window, "destroy", G_CALLBACK(windowShutDown), &workerData);

    GObject* button = gtk_builder_get_object(builder, "resetMinMax");
    g_signal_connect(button, "clicked", G_CALLBACK(requestMinMaxReset), &workerData);

    button = gtk_builder_get_object(builder, "brightnessDown");
    g_signal_connect(button, "clicked", G_CALLBACK(setBrightnessDown), NULL);

    button = gtk_builder_get_object(builder, "brightnessUp");
    g_signal_connect(button, "clicked", G_CALLBACK(setBrightnessUp), NULL);

    button = gtk_builder_get_object(builder, "turnOff");
    g_signal_connect(button, "clicked", G_CALLBACK(buttonShutDown), &workerData);

    gtk_main();

    return 0;
}