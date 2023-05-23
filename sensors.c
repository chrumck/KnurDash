#include <gtk/gtk.h>
#include <pigpiod_if2.h>

#include "dataContracts.h"
#include "helpers.c"
#include "sensorProps.c"
#include "ui.c"

#define SENSOR_WORKER_LOOP_INTERVAL 100000
#define ADC_SWITCH_CHANNEL_SLEEP 5000

#define DRIVE_V 3350

#define FAULTY_READING_LABEL "--"
#define ADC_READING_DEADBAND 10
#define FORMATTED_READING_LENGTH 10

#define ADC0_I2C_ADDRESS 0x6a
#define ADC_DEFAULT_CONFIG 0x10
#define ADC_CHANNEL_MASK 0x60
#define ADC_GET_CHANNEL_VALUE(config) (((config) & ADC_CHANNEL_MASK) >> 5)
#define ADC_GET_CHANNEL_BITS(channel) (((channel) << 5) & ADC_CHANNEL_MASK)


#define HANDLE_FAULT()  if (sensorData->isFaulty[channel] == TRUE) return; \
                        SetLabelArgs args = { .label = sensorData->widgets[channel].label, .value = FAULTY_READING_LABEL }; \
                        g_idle_add(setLabelText, &args); \
                        sensorData->isFaulty[channel] = TRUE; \

static void readChannel(SensorData* sensorData, int channel) {

    guint8 newConfig = ADC_DEFAULT_CONFIG | ADC_GET_CHANNEL_BITS(channel);
    int writeResult = i2c_write_byte(sensorData->piHandle, sensorData->adcHandle, newConfig);
    if (writeResult < 0) {
        HANDLE_FAULT();
        g_warning("Could not write config to adc: %d", writeResult);
        return;
    }

    g_usleep(ADC_SWITCH_CHANNEL_SLEEP);

    guint8 buf[3];
    int readResult = i2c_read_device(sensorData->piHandle, sensorData->adcHandle, buf, 3);
    if (readResult != 3) {
        HANDLE_FAULT();
        g_warning("Could not read adc bytes: %d", readResult);
        return;
    }

    int receivedChannel = ADC_GET_CHANNEL_VALUE(buf[2]);
    if (receivedChannel != channel) {
        HANDLE_FAULT();
        g_warning("Channel received %d does not match required: %d", receivedChannel, channel);
        return;
    }

    const guint32 temp = buf[0] << 8 | buf[1];
    const gint32 sensorV = signExtend32(temp, 12);

    const Sensor sensor = sensors[channel];
    gboolean isFaulty = sensorData->isFaulty[channel];

    if ((isFaulty == FALSE && (sensorV < sensor.vMin || sensorV > sensor.vMax)) ||
        (isFaulty == TRUE && (sensorV < sensor.vMin + ADC_READING_DEADBAND || sensorV > sensor.vMax - ADC_READING_DEADBAND))) {
        HANDLE_FAULT();
        g_warning("Raw value %d out of bounds: %d - %d", sensorV, sensor.vMin, sensor.vMax);
        return;
    }

    gdouble reading = sensor.convert(sensorV, DRIVE_V, sensor.refR);

    sensorData->lastReadings[channel] = reading;
    sensorData->isFaulty[channel] = FALSE;

    char formatted[FORMATTED_READING_LENGTH];
    sprintf(formatted, sensor.format, reading);
    SetLabelArgs args = { .label = sensorData->widgets[channel].label, .value = formatted };
    g_idle_add(setLabelText, &args);
}

static gpointer sensorWorkerLoop(gpointer data) {
    WorkerData* workerData = (WorkerData*)data;

    int piHandle = pigpio_start(NULL, NULL);
    if (piHandle < 0)  g_error("Could not connect to pigpiod: %d", piHandle);

    int adc0Handle = i2c_open(piHandle, 1, ADC0_I2C_ADDRESS, 0);
    if (adc0Handle < 0)  g_error("Could not get adc0 handle %d", adc0Handle);

    SensorData sensorData = { .piHandle = piHandle, .adcHandle = adc0Handle };
    for (int i = 0; i < getSize(sensors); i++)
    {
        const Sensor sensor = sensors[i];
        sensorData.widgets[i].label = GTK_LABEL(gtk_builder_get_object(workerData->builder, sensor.labelId));
        sensorData.widgets[i].frame = GTK_FRAME(gtk_builder_get_object(workerData->builder, sensor.frameId));
        sensorData.widgets[i].labelMin = GTK_LABEL(gtk_builder_get_object(workerData->builder, sensor.labelMinId));
        sensorData.widgets[i].labelMax = GTK_LABEL(gtk_builder_get_object(workerData->builder, sensor.labelMaxId));

        sensorData.minValues[i] = G_MAXDOUBLE;
        sensorData.maxValues[i] = G_MINDOUBLE;
    }

    g_message("Sensor worker starting");
    workerData->isSensorWorkerRunning = TRUE;
    while (workerData->requestShutdown == FALSE) {
        readChannel(&sensorData, 2);
        readChannel(&sensorData, 3);

        g_usleep(SENSOR_WORKER_LOOP_INTERVAL);
    }

    g_message("Sensor worker shutting down");

    i2c_close(piHandle, adc0Handle);
    pigpio_stop(piHandle);

    workerData->isSensorWorkerRunning = FALSE;

    return NULL;
}