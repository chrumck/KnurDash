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

#define ADC0_I2C_ADDRESS 0x6a
#define ADC_DEFAULT_CONFIG 0x10
#define ADC_CHANNEL_MASK 0x60
#define getAdcChannelValue(config) (((config) & ADC_CHANNEL_MASK) >> 5)
#define getAdcChannelBits(channel) (((channel) << 5) & ADC_CHANNEL_MASK)

#define handleFault()\
    if (wasFaulty == TRUE) return;\
    sensorData->isFaulty[channel] = TRUE;\
    setLabel(sensorData->widgets[channel].label, FAULTY_READING_LABEL, 0);\
    setFrame(sensorData->widgets[channel].frame, StateNormal);\

//-------------------------------------------------------------------------------------------------------------

static void setLabel(GtkLabel* label, const char* format, gdouble reading) {
    gpointer data = malloc(sizeof(SetLabelArgs));
    SetLabelArgs* args = (SetLabelArgs*)data;
    args->label = label;
    sprintf(args->value, format, reading);
    g_idle_add(setLabelText, data);
}

static void setFrame(GtkFrame* frame, const SensorState state) {
    gpointer data = malloc(sizeof(SetFrameClassArgs));
    SetFrameClassArgs* args = (SetFrameClassArgs*)data;
    args->frame = frame;
    args->state = state;
    g_idle_add(setFrameClass, data);
}

static void resetLastReadings(SensorData* sensorData) {
    for (int i = 0; i < getSize(sensors); i++) {
        sensorData->lastReadings[i] = G_MAXDOUBLE;
    }
};

static void resetMinMaxReadings(SensorData* sensorData) {
    for (int i = 0; i < getSize(sensors); i++) {
        sensorData->minValues[i] = G_MAXDOUBLE;
        sensorData->maxValues[i] = -G_MAXDOUBLE;
    }
};

static void resetMinMaxLabels(SensorData* sensorData) {
    for (int i = 0; i < getSize(sensors); i++) {
        if (sensorData->widgets[i].labelMin != 0) setLabel(sensorData->widgets[i].labelMin, FAULTY_READING_LABEL, 0);
        if (sensorData->widgets[i].labelMax != 0)setLabel(sensorData->widgets[i].labelMax, FAULTY_READING_LABEL, 0);
    }
};

static SensorState getSensorState(const Sensor* sensor, const gdouble reading) {
    if (reading < sensor->alertLow) return StateAlertLow;
    if (reading < sensor->warningLow) return StateWarningLow;
    if (reading < sensor->notifyLow) return StateNotifyLow;

    if (reading > sensor->alertHigh) return StateAlertHigh;
    if (reading > sensor->warningHigh) return StateWarningHigh;
    if (reading > sensor->notifyHigh) return StateNotifyLow;

    return StateNormal;
}

//-------------------------------------------------------------------------------------------------------------

static void readChannel(SensorData* sensorData, int channel) {
    const Sensor* sensor = &sensors[channel];
    gboolean wasFaulty = sensorData->isFaulty[channel];

    guint8 newConfig = ADC_DEFAULT_CONFIG | getAdcChannelBits(channel);
    int writeResult = i2c_write_byte(sensorData->piHandle, sensorData->adcHandle, newConfig);
    if (writeResult < 0) {
        handleFault();
        g_warning("Could not write config to adc: %d", writeResult);
        return;
    }

    g_usleep(ADC_SWITCH_CHANNEL_SLEEP);

    guint8 buf[3];
    int readResult = i2c_read_device(sensorData->piHandle, sensorData->adcHandle, buf, 3);
    if (readResult != 3) {
        handleFault();
        g_warning("Could not read adc bytes: %d", readResult);
        return;
    }

    int receivedChannel = getAdcChannelValue(buf[2]);
    if (receivedChannel != channel) {
        handleFault();
        g_warning("Channel received %d does not match required: %d", receivedChannel, channel);
        return;
    }

    const guint32 temp = buf[0] << 8 | buf[1];
    const gint32 sensorV = signExtend32(temp, 12);

    if ((wasFaulty == FALSE && (sensorV < sensor->vMin || sensorV > sensor->vMax)) ||
        (wasFaulty == TRUE && (sensorV < sensor->vMin + ADC_READING_DEADBAND || sensorV > sensor->vMax - ADC_READING_DEADBAND))) {
        handleFault();
        g_warning("Raw value %d out of bounds: %d - %d", sensorV, sensor->vMin, sensor->vMax);
        return;
    }

    sensorData->isFaulty[channel] = FALSE;
    const gdouble reading = sensor->convert(sensorV, DRIVE_V, sensor->refR);

    if (wasFaulty == FALSE &&
        reading < sensorData->lastReadings[channel] + sensor->precision &&
        reading > sensorData->lastReadings[channel] - sensor->precision &&
        reading >= sensorData->minValues[channel] - sensor->precision &&
        reading <= sensorData->maxValues[channel] + sensor->precision) return;

    sensorData->lastReadings[channel] = reading;
    setLabel(sensorData->widgets[channel].label, sensor->format, reading);

    if (sensorData->minValues[channel] > reading) {
        sensorData->minValues[channel] = reading;
        setLabel(sensorData->widgets[channel].labelMin, sensor->format, reading);
    }

    if (sensorData->maxValues[channel] < reading) {
        sensorData->maxValues[channel] = reading;
        setLabel(sensorData->widgets[channel].labelMax, sensor->format, reading);
    }

    const SensorState state = getSensorState(sensor, reading);
    if (wasFaulty == TRUE || state != sensorData->lastStates[channel]) {
        sensorData->lastStates[channel] = state;
        setFrame(sensorData->widgets[channel].frame, state);
    }
}

static gpointer sensorWorkerLoop(gpointer data) {
    WorkerData* workerData = (WorkerData*)data;

    int piHandle = pigpio_start(NULL, NULL);
    if (piHandle < 0)  g_error("Could not connect to pigpiod: %d", piHandle);

    int adc0Handle = i2c_open(piHandle, 1, ADC0_I2C_ADDRESS, 0);
    if (adc0Handle < 0)  g_error("Could not get adc0 handle %d", adc0Handle);

    SensorData sensorData = { .piHandle = piHandle, .adcHandle = adc0Handle };
    resetLastReadings(&sensorData);
    resetMinMaxReadings(&sensorData);

    for (int i = 0; i < getSize(sensors); i++) {
        const Sensor* sensor = &sensors[i];
        sensorData.widgets[i].label = GTK_LABEL(gtk_builder_get_object(workerData->builder, sensor->labelId));
        sensorData.widgets[i].frame = GTK_FRAME(gtk_builder_get_object(workerData->builder, sensor->frameId));
        sensorData.widgets[i].labelMin = GTK_LABEL(gtk_builder_get_object(workerData->builder, sensor->labelMinId));
        sensorData.widgets[i].labelMax = GTK_LABEL(gtk_builder_get_object(workerData->builder, sensor->labelMaxId));
    }

    g_message("Sensor worker starting");
    workerData->isSensorWorkerRunning = TRUE;
    while (workerData->requestShutdown == FALSE) {
        if (workerData->requestMinMaxReset == TRUE) {
            resetMinMaxReadings(&sensorData);
            resetMinMaxLabels(&sensorData);
            workerData->requestMinMaxReset = FALSE;
        }

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