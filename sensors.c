#include <gtk/gtk.h>
#include <pigpiod_if2.h>
#include <pigpio.h>

static gpointer sensorWorkerLoop(gpointer data) {
    WorkerData* workerData = (WorkerData*)data;


    const GObject* oilTempLabel = gtk_builder_get_object(workerData->builder, "oilTemp");
    const GObject* oilTempMinLabel = gtk_builder_get_object(workerData->builder, "oilTempMin");
    const GObject* oilTempMaxLabel = gtk_builder_get_object(workerData->builder, "oilTempMax");
    const GObject* oilPressLabel = gtk_builder_get_object(workerData->builder, "oilPress");
    const GObject* oilPressMinLabel = gtk_builder_get_object(workerData->builder, "oilPressMin");
    const GObject* oilPressMaxLabel = gtk_builder_get_object(workerData->builder, "oilPressMax");

    double oilTempValue = DBL_MAX;
    double oilTempMinValue = DBL_MAX;
    double oilTempMaxValue = DBL_MAX;
    double oilPressValue = DBL_MAX;
    double oilPressMinValue = DBL_MAX;
    double oilPressMaxValue = DBL_MAX;

    int pi = pigpio_start(NULL, NULL);
    if (pi < 0)  g_error("Could not connect to pigpiod");

    int adcHandle = i2cOpen(1, 0xaa, 0);
    if (adcHandle < 0) {
        g_warning("Could not connect to ADC", adcHandle);
        pigpio_stop(pi);
        return NULL;
    }

    g_message("Sensor worker started");

    workerData->isSensorWorkerRunning = TRUE;
    while (workerData->isShuttingDown == FALSE) {

        g_usleep(3000000);

        g_message("Ping from analog sensors loop");

    }

    g_message("Sensor worker shutting down");

    pigpio_stop(pi);

    workerData->isSensorWorkerRunning = FALSE;

    return NULL;
}