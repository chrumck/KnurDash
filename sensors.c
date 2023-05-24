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
#define ADC1_I2C_ADDRESS 0x6c
#define ADC_DEFAULT_CONFIG 0x10
#define ADC_CHANNEL_MASK 0x60
#define getAdcChannelValue(config) (((config) & ADC_CHANNEL_MASK) >> 5)
#define getAdcChannelBits(channel) (((channel) << 5) & ADC_CHANNEL_MASK)

#define handleFault()\
    if (wasFaulty == TRUE) return;\
    reading->isFaulty = TRUE;\
    setLabel(widgets->label, FAULTY_READING_LABEL, 0);\
    setFrame(widgets->frame, StateNormal);\

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
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            sensorData->readings[i][j].value = G_MAXDOUBLE;
        }
    }
};

static void resetMinMaxReadings(SensorData* sensorData) {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            sensorData->readings[i][j].min = G_MAXDOUBLE;
            sensorData->readings[i][j].max = -G_MAXDOUBLE;
        }
    }
};

static void setWidgets(GtkBuilder* builder, SensorData* sensorData) {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            const Sensor* sensor = &sensors[i][j];
            SensorWidgets* widgets = &sensorData->widgets[i][j];
            if (sensor->labelId != NULL) widgets->label = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelId));
            if (sensor->frameId != NULL) widgets->frame = GTK_FRAME(gtk_builder_get_object(builder, sensor->frameId));
            if (sensor->labelMinId != NULL) widgets->labelMin = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelMinId));
            if (sensor->labelMaxId != NULL) widgets->labelMax = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelMaxId));
        }
    }
};

static void resetMinMaxLabels(SensorData* sensorData) {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            const SensorWidgets* widgets = &sensorData->widgets[i][j];
            if (widgets->labelMin != NULL) setLabel(widgets->labelMin, FAULTY_READING_LABEL, 0);
            if (widgets->labelMax != NULL) setLabel(widgets->labelMax, FAULTY_READING_LABEL, 0);
        }
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

static void readChannel(SensorData* sensorData, int adc, int channel) {
    const Sensor* sensor = &sensors[adc][channel];
    const SensorWidgets* widgets = &sensorData->widgets[adc][channel];
    SensorReading* reading = &sensorData->readings[adc][channel];
    gboolean wasFaulty = reading->isFaulty;

    guint8 newConfig = ADC_DEFAULT_CONFIG | getAdcChannelBits(channel);
    int writeResult = i2c_write_byte(sensorData->piHandle, sensorData->adcHandle[adc], newConfig);
    if (writeResult < 0) {
        handleFault();
        g_warning("Could not write config to adc: %d", writeResult);
        return;
    }

    g_usleep(ADC_SWITCH_CHANNEL_SLEEP);

    guint8 buf[3];
    int readResult = i2c_read_device(sensorData->piHandle, sensorData->adcHandle[adc], buf, 3);
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
    const gint32 v = signExtend32(temp, 12);

    if ((wasFaulty == FALSE && (v < sensor->vMin || v > sensor->vMax)) ||
        (wasFaulty == TRUE && (v < sensor->vMin + ADC_READING_DEADBAND || v > sensor->vMax - ADC_READING_DEADBAND))) {
        handleFault();
        g_warning("Raw value %d out of bounds: %d - %d", v, sensor->vMin, sensor->vMax);
        return;
    }

    reading->isFaulty = FALSE;
    const gdouble value = sensor->convert(v, DRIVE_V, sensor->refR);

    if (wasFaulty == FALSE &&
        value < reading->value + sensor->precision &&
        value > reading->value - sensor->precision &&
        value >= reading->min - sensor->precision &&
        value <= reading->max + sensor->precision) return;

    reading->value = value;
    setLabel(widgets->label, sensor->format, value);

    if (reading->min > value) {
        reading->min = value;
        setLabel(widgets->labelMin, sensor->format, value);
    }

    if (reading->max < value) {
        reading->max = value;
        setLabel(widgets->labelMax, sensor->format, value);
    }

    const SensorState state = getSensorState(sensor, value);
    if (wasFaulty == TRUE || state != reading->state) {
        reading->state = state;
        setFrame(widgets->frame, state);
    }
}

static gpointer sensorWorkerLoop(gpointer data) {
    WorkerData* workerData = (WorkerData*)data;

    int piHandle = pigpio_start(NULL, NULL);
    if (piHandle < 0)  g_error("Could not connect to pigpiod: %d", piHandle);

    int adc0Handle = i2c_open(piHandle, 1, ADC0_I2C_ADDRESS, 0);
    if (adc0Handle < 0)  g_error("Could not get adc0 handle %d", adc0Handle);

    int adc1Handle = i2c_open(piHandle, 1, ADC1_I2C_ADDRESS, 0);
    if (adc1Handle < 0)  g_error("Could not get adc1 handle %d", adc1Handle);

    SensorData sensorData = { .piHandle = piHandle, .adcHandle = {adc0Handle, adc1Handle} };
    resetLastReadings(&sensorData);
    resetMinMaxReadings(&sensorData);
    setWidgets(workerData->builder, &sensorData);

    g_message("Sensor worker starting");
    workerData->isSensorWorkerRunning = TRUE;
    while (workerData->requestShutdown == FALSE) {
        if (workerData->requestMinMaxReset == TRUE) {
            resetMinMaxReadings(&sensorData);
            resetMinMaxLabels(&sensorData);
            workerData->requestMinMaxReset = FALSE;
        }

        readChannel(&sensorData, 0, 2);
        readChannel(&sensorData, 0, 3);

        g_usleep(SENSOR_WORKER_LOOP_INTERVAL);
    }

    g_message("Sensor worker shutting down");

    i2c_close(piHandle, adc0Handle);
    pigpio_stop(piHandle);

    workerData->isSensorWorkerRunning = FALSE;

    return NULL;
}