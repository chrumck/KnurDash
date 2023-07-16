#include <gtk/gtk.h>
#include <pigpiod_if2.h>

#include "dataContracts.h"
#include "workerData.c"
#include "helpers.c"
#include "sensorProps.c"
#include "canBusProps.c"
#include "ui.c"

#define SENSOR_WORKER_LOOP_INTERVAL 100e3
#define ADC_SWITCH_CHANNEL_SLEEP 5e3
#define SHUTDOWN_DELAY 600
#define SHUTDOWN_DELAY_ENGINE_STARTED 30
#define OIL_PRESSURE_ALERT_MIN_RPM 1000

#define IGN_GPIO_PIN 22
#define BUZZER_GPIO_PIN 27

#define VDD_DEFAULT 3350
#define VDD_ADC 1
#define VDD_CHANNEL 3

#define OIL_TEMP_ADC 0
#define OIL_TEMP_CHANNEL 2
#define OIL_PRESS_ADC 0
#define OIL_PRESS_CHANNEL 3

#define FAULTY_READING_LABEL "--"
#define FAULTY_READING_VALUE (G_MAXDOUBLE - 10e3)
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
    reading->value = FAULTY_READING_VALUE;\
    setLabel(widgets->label, FAULTY_READING_LABEL, 0);\
    setFrame(widgets->frame, StateNormal);\

//-------------------------------------------------------------------------------------------------------------

void setLabel(GtkLabel* label, const char* format, gdouble reading) {
    if (label == NULL) return;
    gpointer data = malloc(sizeof(SetLabelArgs));
    SetLabelArgs* args = (SetLabelArgs*)data;
    args->label = label;
    sprintf(args->value, format, reading);
    g_idle_add(setLabelText, data);
}

void setFrame(GtkFrame* frame, const SensorState state) {
    if (frame == NULL) return;
    gpointer data = malloc(sizeof(SetFrameClassArgs));
    SetFrameClassArgs* args = (SetFrameClassArgs*)data;
    args->frame = frame;
    args->state = state;
    g_idle_add(setFrameClass, data);
}

void resetReadingsValues() {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            workerData.sensorData.readings[i][j].value = FAULTY_READING_VALUE;
            workerData.sensorData.readings[i][j].isFaulty = TRUE;
        }
    }
};

void resetReadingsMinMax() {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            workerData.sensorData.readings[i][j].min = G_MAXDOUBLE;
            workerData.sensorData.readings[i][j].max = -G_MAXDOUBLE;
        }
    }
};

void setWidgets() {
    GtkBuilder* builder = workerData.builder;

    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            const Sensor* sensor = &sensors[i][j];
            SensorWidgets* widgets = &workerData.sensorData.widgets[i][j];
            if (sensor->labelId != NULL) widgets->label = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelId));
            if (sensor->frameId != NULL) widgets->frame = GTK_FRAME(gtk_builder_get_object(builder, sensor->frameId));
            if (sensor->labelMinId != NULL) widgets->labelMin = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelMinId));
            if (sensor->labelMaxId != NULL) widgets->labelMax = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelMaxId));
        }
    }
};

void resetMinMaxLabels() {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            const SensorWidgets* widgets = &workerData.sensorData.widgets[i][j];
            setLabel(widgets->labelMin, FAULTY_READING_LABEL, 0);
            setLabel(widgets->labelMax, FAULTY_READING_LABEL, 0);
        }
    }
};

SensorState getSensorState(const Sensor* sensor, const gdouble reading) {
    if (reading < sensor->alertLow) return StateAlertLow;
    if (reading < sensor->warningLow) return StateWarningLow;
    if (reading < sensor->notifyLow) return StateNotifyLow;

    if (reading > sensor->alertHigh) return StateAlertHigh;
    if (reading > sensor->warningHigh) return StateWarningHigh;
    if (reading > sensor->notifyHigh) return StateNotifyLow;

    return StateNormal;
}

//-------------------------------------------------------------------------------------------------------------

void readChannel(int adc, int channel) {
    const Sensor* sensor = &sensors[adc][channel];
    SensorData* sensorData = &workerData.sensorData;
    const SensorWidgets* widgets = &sensorData->widgets[adc][channel];
    SensorReading* reading = &sensorData->readings[adc][channel];
    gboolean wasFaulty = reading->isFaulty;

    guint8 newConfig = ADC_DEFAULT_CONFIG | getAdcChannelBits(channel);
    int writeResult = i2c_write_byte(sensorData->i2cPiHandle, sensorData->i2cAdcHandles[adc], newConfig);
    if (writeResult != 0) {
        handleFault();
        g_warning("Could not write config to adc: %d - adc:%d, channel:%d", writeResult, adc, channel);
        return;
    }

    g_usleep(ADC_SWITCH_CHANNEL_SLEEP);

    guint8 buf[3];
    int readResult = i2c_read_device(sensorData->i2cPiHandle, sensorData->i2cAdcHandles[adc], buf, 3);
    if (readResult != 3) {
        handleFault();
        g_warning("Could not read adc bytes: %d - adc:%d, channel:%d", readResult, adc, channel);
        return;
    }

    int receivedChannel = getAdcChannelValue(buf[2]);
    if (receivedChannel != channel) {
        handleFault();
        g_warning("Channel received %d does not match required - adc:%d, channel:%d", receivedChannel, adc, channel);
        return;
    }

    const guint32 temp = buf[0] << 8 | buf[1];
    const gint32 v = signExtend32(temp, 12);

    if ((wasFaulty == FALSE && (v < sensor->vMin || v > sensor->vMax)) ||
        (wasFaulty == TRUE && (v < sensor->vMin + ADC_READING_DEADBAND || v > sensor->vMax - ADC_READING_DEADBAND))) {
        handleFault();
        g_warning("Raw value %d out of bounds: %d~%d - adc:%d, channel:%d ", v, sensor->vMin, sensor->vMax, adc, channel);
        return;
    }

    SensorReading* vddReading = &sensorData->readings[VDD_ADC][VDD_CHANNEL];
    const gdouble vdd = vddReading->isFaulty == FALSE ? vddReading->value : VDD_DEFAULT;

    const gdouble value = sensor->convert(v, (int)vdd, sensor->refR);
    reading->isFaulty = FALSE;

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

gpointer sensorWorkerLoop() {
    g_message("Sensor worker starting");

    int i2cPiHandle = pigpio_start(NULL, NULL);
    if (i2cPiHandle < 0)  g_error("Could not connect to pigpiod: %d", i2cPiHandle);
    workerData.sensorData.i2cPiHandle = i2cPiHandle;

    int adc0Handle = i2c_open(i2cPiHandle, 1, ADC0_I2C_ADDRESS, 0);
    if (adc0Handle < 0)  g_error("Could not get adc0 handle: %d", adc0Handle);
    workerData.sensorData.i2cAdcHandles[0] = adc0Handle;

    int adc1Handle = i2c_open(i2cPiHandle, 1, ADC1_I2C_ADDRESS, 0);
    if (adc1Handle < 0)  g_error("Could not get adc1 handle: %d", adc1Handle);
    workerData.sensorData.i2cAdcHandles[1] = adc1Handle;

    int setIgnPinMode = set_mode(i2cPiHandle, IGN_GPIO_PIN, PI_INPUT);
    if (setIgnPinMode != 0) g_error("Could not set GPIO pin mode for IGN_IN: %d", setIgnPinMode);

    int setIgnPinPullDown = set_pull_up_down(i2cPiHandle, IGN_GPIO_PIN, PI_PUD_DOWN);
    if (setIgnPinPullDown != 0) g_error("Could not set GPIO pin pulldown for IGN_IN: %d", setIgnPinPullDown);

    int setBuzzerPinMode = set_mode(i2cPiHandle, BUZZER_GPIO_PIN, PI_OUTPUT);
    if (setBuzzerPinMode != 0) g_error("Could not set GPIO pin mode for BUZZER: %d", setBuzzerPinMode);

    int setBuzzerPinPullDown = set_pull_up_down(i2cPiHandle, BUZZER_GPIO_PIN, PI_PUD_OFF);
    if (setBuzzerPinPullDown != 0) g_error("Could not set GPIO pin pulldown for BUZZER: %d", setBuzzerPinPullDown);

    resetReadingsValues();
    resetReadingsMinMax();
    setWidgets();

    workerData.isSensorWorkerRunning = TRUE;
    guint shutDownCounter = 0;

    while (workerData.requestShutdown == FALSE) {
        if (workerData.requestMinMaxReset == TRUE) {
            resetReadingsMinMax();
            resetMinMaxLabels();
            workerData.requestMinMaxReset = FALSE;
        }

        gboolean ignOn = gpio_read(i2cPiHandle, IGN_GPIO_PIN);

        SensorReading* pressureReading = &(workerData.sensorData.readings)[OIL_PRESS_ADC][OIL_PRESS_CHANNEL];
        const Sensor* pressureSensor = &sensors[OIL_PRESS_ADC][OIL_PRESS_CHANNEL];

        if (workerData.wasEngineStarted == FALSE &&
            ignOn == TRUE &&
            pressureReading->isFaulty == FALSE &&
            pressureReading->value > pressureSensor->alertLow) {
            workerData.wasEngineStarted = TRUE;
        }

        gint32 engineRpm = getEngineRpm();
        gboolean buzzerOn = gpio_read(i2cPiHandle, BUZZER_GPIO_PIN) == TRUE;

        if (engineRpm < OIL_PRESSURE_ALERT_MIN_RPM && buzzerOn) gpio_write(i2cPiHandle, BUZZER_GPIO_PIN, FALSE);

        if (pressureReading->isFaulty == FALSE &&
            pressureReading->value < pressureSensor->alertLow &&
            engineRpm > OIL_PRESSURE_ALERT_MIN_RPM &&
            !buzzerOn) {
            gpio_write(i2cPiHandle, BUZZER_GPIO_PIN, TRUE);
        }

        shutDownCounter = ignOn == TRUE ? 0 : shutDownCounter + 1;

#ifdef NDEBUG
        if ((workerData.wasEngineStarted == TRUE && shutDownCounter > SHUTDOWN_DELAY_ENGINE_STARTED) ||
            shutDownCounter > SHUTDOWN_DELAY) {
            g_idle_add(systemShutDown, NULL);
            break;
        }
#endif

        readChannel(VDD_ADC, VDD_CHANNEL);

        readChannel(OIL_TEMP_ADC, OIL_TEMP_CHANNEL);
        readChannel(OIL_PRESS_ADC, OIL_PRESS_CHANNEL);

        g_usleep(SENSOR_WORKER_LOOP_INTERVAL);
    }

    gpio_write(i2cPiHandle, BUZZER_GPIO_PIN, FALSE);
    i2c_close(i2cPiHandle, adc0Handle);
    i2c_close(i2cPiHandle, adc1Handle);
    pigpio_stop(i2cPiHandle);

    workerData.isSensorWorkerRunning = FALSE;
    g_message("Sensor worker shutting down");

    return NULL;
}