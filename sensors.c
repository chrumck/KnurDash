#include <gtk/gtk.h>
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

    if (gpioInitialise() < 0)  g_error("Could not initialize GPIO");

    g_message("Sensor worker started");

    workerData->isSensorWorkerRunning = TRUE;
    while (workerData->isShuttingDown == FALSE) {

        g_usleep(3000000);

        g_message("Ping from analog sensors loop");

    }

    g_message("Sensor worker shutting down");

    gpioTerminate();

    workerData->isSensorWorkerRunning = FALSE;

    return NULL;
}