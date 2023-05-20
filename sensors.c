#include <gtk/gtk.h>

#include <pigpio.h>

gpointer readAnalogSensors(gpointer data) {
    GtkBuilder* builder = (GtkBuilder*)data;

    GObject* oilTempLabel = gtk_builder_get_object(builder, "oilTemp");
    GObject* oilTempMinLabel = gtk_builder_get_object(builder, "oilTempMin");
    GObject* oilTempMaxLabel = gtk_builder_get_object(builder, "oilTempMax");
    GObject* oilPressLabel = gtk_builder_get_object(builder, "oilPress");
    GObject* oilPressMinLabel = gtk_builder_get_object(builder, "oilPressMin");
    GObject* oilPressMaxLabel = gtk_builder_get_object(builder, "oilPressMax");

    double oilTempValue = DBL_MAX;
    double oilTempMinValue = DBL_MAX;
    double oilTempMaxValue = DBL_MAX;
    double oilPressValue = DBL_MAX;
    double oilPressMinValue = DBL_MAX;
    double oilPressMaxValue = DBL_MAX;

    // if (gpioInitialise() < 0) {
    //     g_error("Could not initialize pigpio");
    // }


    while (TRUE) {

        g_usleep(3000000);

        g_message("Hello from analog sensors loop");

    }

    return NULL;
}