#include <gtk/gtk.h>
#include <pigpiod_if2.h>

#include "dataContracts.h"
#include "helpers.c"

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
    if (pi < 0)  g_error("Could not connect to pigpiod", pi);

    int adc = i2c_open(pi, 1, 0x6e, 0);
    if (adc < 0)  g_error("Could not get adc handle", adc);

    g_message("Sensor worker started");

    guint8 buf[3] = { 0, 0, 0 };
    guint32 temp;
    guint8 config = 0;

    workerData->isSensorWorkerRunning = TRUE;
    while (workerData->isShuttingDown == FALSE) {
        g_usleep(1000000);

        int bytesRead = i2c_read_device(pi, adc, buf, 3);
        if (bytesRead != 3)  g_warning("Could not read adc bytes: %d", bytesRead);


        temp = buf[0] << 8 | buf[1];
        config = buf[2];

        gint32 result = sign_extend32(temp, 12);
        // g_message("Read %d", result);



    }

    g_message("Sensor worker shutting down");

    i2c_close(pi, adc);
    pigpio_stop(pi);

    workerData->isSensorWorkerRunning = FALSE;

    return NULL;
}