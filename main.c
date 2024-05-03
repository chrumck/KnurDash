#define G_LOG_USE_STRUCTURED

#include <gtk/gtk.h>
#include <signal.h>
#include <glib-unix.h>

#include "dataContracts.h"
#include "appData.c"
#include "ui.c"
#include "sensorWorker.c"
#include "canBusWorker.c"
#include "bluetoothWorker.c"
#include "systemWorker.c"

#define FILE_PATH_LENGTH (PATH_MAX + 1)

int main(int argc, char* argv[])
{
    appData.startupTimestamp = g_get_monotonic_time();

    gtk_init(&argc, &argv);

    char exeFilePath[FILE_PATH_LENGTH];
    readlink("/proc/self/exe", exeFilePath, FILE_PATH_LENGTH);

    char cssFilePath[FILE_PATH_LENGTH];
    sprintf(cssFilePath, "%s%s", exeFilePath, ".css");

    GError* error = NULL;
    GtkCssProvider* cssProvider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(cssProvider, cssFilePath, &error);

    if (error != NULL) g_error("Error loading file: %s\n", error->message);

    GdkScreen* screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(screen, GTK_STYLE_PROVIDER(cssProvider), GTK_STYLE_PROVIDER_PRIORITY_USER);

    char uiFilePath[FILE_PATH_LENGTH];
    sprintf(uiFilePath, "%s%s", exeFilePath, ".ui");

    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, uiFilePath, &error);
    if (error != NULL) g_error("Error loading file: %s\n", error->message);

    GObject* window = gtk_builder_get_object(builder, "window");

    gtk_window_fullscreen(GTK_WINDOW(window));

    appData.builder = builder;

    GThread* sensorWorker = g_thread_new("readAnalogSensors", sensorWorkerLoop, NULL);
    GThread* canBusWorker = g_thread_new("canBusWorker", canBusWorkerLoop, NULL);
    GThread* bluetoothWorker = g_thread_new("bluetoothWorker", bluetoothWorkerLoop, NULL);
    GThread* systemWorker = g_thread_new("systemWorker", systemWorkerLoop, NULL);

    g_unix_signal_add(SIGINT, shutDown, GUINT_TO_POINTER(AppShutdown));
    g_unix_signal_add(SIGTERM, shutDown, GUINT_TO_POINTER(AppShutdown));

    g_signal_connect(window, "destroy", G_CALLBACK(shutDown), GUINT_TO_POINTER(AppShutdown));

    GObject* button = gtk_builder_get_object(builder, "resetMinMax");
    g_signal_connect(button, "clicked", G_CALLBACK(requestMinMaxReset), NULL);

    button = gtk_builder_get_object(builder, "brightnessDown");
    g_signal_connect(button, "clicked", G_CALLBACK(setBrightnessDown), NULL);

    button = gtk_builder_get_object(builder, "brightnessUp");
    g_signal_connect(button, "clicked", G_CALLBACK(setBrightnessUp), NULL);

    button = gtk_builder_get_object(builder, "turnOff");
    g_signal_connect(button, "clicked", G_CALLBACK(buttonShutDown), NULL);

    gtk_main();

    g_thread_join(sensorWorker);
    g_thread_join(canBusWorker);
    g_thread_join(bluetoothWorker);
    g_thread_join(systemWorker);

    g_message("KnurDash app terminated");

    return 0;
}