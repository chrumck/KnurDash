#include <gtk/gtk.h>
#include <pigpiod_if2.h>

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

    g_message("Sensor worker started");

    workerData->isSensorWorkerRunning = TRUE;
    int adc = -1;

    while (workerData->isShuttingDown == FALSE) {
        if (adc < 0) adc = i2c_open(pi, 1, 0x6e, 0);
        if (adc < 0) {
            g_warning("Could not connect to ADC", adc);
            continue;;
        }



        g_usleep(100000);
    }

    g_message("Sensor worker shutting down");

    if (adc >= 0) i2c_close(pi, adc);
    pigpio_stop(pi);

    workerData->isSensorWorkerRunning = FALSE;

    return NULL;
}