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
#define OIL_PRESSURE_ALERT_MIN_RPM 1300

#define IGN_GPIO_PIN 22
#define BUZZER_GPIO_PIN 27

#define COOLANT_TEMP_CAN_SENSOR_INDEX 0

#define NO_READING_LABEL "--"
#define FAULTY_READING_VALUE (G_MAXDOUBLE - 10e3)
#define ADC_READING_DEADBAND 10

#define ADC0_I2C_ADDRESS 0x6a
#define ADC1_I2C_ADDRESS 0x6c
#define ADC_DEFAULT_CONFIG 0x10
#define ADC_CHANNEL_MASK 0x60
#define getAdcChannelValue(config) (((config) & ADC_CHANNEL_MASK) >> 5)
#define getAdcChannelBits(channel) (((channel) << 5) & ADC_CHANNEL_MASK)

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
            SensorReading* reading = &workerData.sensors.adcReadings[i][j];
            reading->value = FAULTY_READING_VALUE;
            reading->isFaulty = TRUE;
        }
    }
    for (int i = 0; i < CAN_SENSORS_COUNT; i++) {
        SensorReading* reading = &workerData.sensors.canReadings[i];
        reading->value = FAULTY_READING_VALUE;
        reading->isFaulty = TRUE;
    }
};

void resetReadingsMinMax() {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            workerData.sensors.adcReadings[i][j].min = G_MAXDOUBLE;
            workerData.sensors.adcReadings[i][j].max = -G_MAXDOUBLE;
        }
    }
    for (int i = 0; i < CAN_SENSORS_COUNT; i++) {
        workerData.sensors.canReadings[i].min = G_MAXDOUBLE;
        workerData.sensors.canReadings[i].max = -G_MAXDOUBLE;
    }
};

void setSingleSensorWidgets(const SensorBase* sensor, SensorWidgets* widgets) {
    GtkBuilder* builder = workerData.builder;
    if (sensor->labelId != NULL) widgets->label = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelId));
    if (sensor->frameId != NULL) widgets->frame = GTK_FRAME(gtk_builder_get_object(builder, sensor->frameId));
    if (sensor->labelMinId != NULL) widgets->labelMin = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelMinId));
    if (sensor->labelMaxId != NULL) widgets->labelMax = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelMaxId));
}

void setWidgets() {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            const SensorBase* sensor = &adcSensors[i][j].base;
            SensorWidgets* widgets = &workerData.sensors.adcWidgets[i][j];
            setSingleSensorWidgets(sensor, widgets);
        }
    }
    for (int i = 0; i < CAN_SENSORS_COUNT; i++) {
        const SensorBase* sensor = &canSensors[i].base;
        SensorWidgets* widgets = &workerData.sensors.canWidgets[i];
        setSingleSensorWidgets(sensor, widgets);
    }
};

void resetMinMaxLabels() {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            const SensorWidgets* widgets = &workerData.sensors.adcWidgets[i][j];
            setLabel(widgets->labelMin, NO_READING_LABEL, 0);
            setLabel(widgets->labelMax, NO_READING_LABEL, 0);
        }
    }
    for (int i = 0; i < CAN_SENSORS_COUNT; i++) {
        const SensorWidgets* widgets = &workerData.sensors.canWidgets[i];
        setLabel(widgets->labelMin, NO_READING_LABEL, 0);
        setLabel(widgets->labelMax, NO_READING_LABEL, 0);

    }
};

SensorState getSensorState(const SensorBase* sensor, const gdouble reading) {
    if (reading < sensor->alertLow) return StateAlertLow;
    if (reading < sensor->warningLow) return StateWarningLow;
    if (reading < sensor->notifyLow) return StateNotifyLow;

    if (reading > sensor->alertHigh) return StateAlertHigh;
    if (reading > sensor->warningHigh) return StateWarningHigh;
    if (reading > sensor->notifyHigh) return StateNotifyLow;

    return StateNormal;
}

#define handleSensorReadFault()\
    if (reading->isFaulty) return;\
    reading->isFaulty = TRUE;\
    reading->value = FAULTY_READING_VALUE;\
    setLabel(widgets->label, NO_READING_LABEL, 0);\
    setFrame(widgets->frame, StateNormal);\

void setSensorReadingAndWidgets(
    gdouble value,
    SensorReading* reading,
    const SensorBase* sensor,
    const SensorWidgets* widgets) {

    if (!reading->isFaulty && value < reading->value + sensor->precision && value > reading->value - sensor->precision) return;

    gboolean wasFaulty = reading->isFaulty;

    reading->isFaulty = FALSE;
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

//-------------------------------------------------------------------------------------------------------------

void readAdcSensor(int adc, int channel) {
    const AdcSensor* sensor = &adcSensors[adc][channel];
    SensorData* sensors = &workerData.sensors;
    const SensorWidgets* widgets = &sensors->adcWidgets[adc][channel];
    SensorReading* reading = &sensors->adcReadings[adc][channel];

    guint8 newConfig = ADC_DEFAULT_CONFIG | getAdcChannelBits(channel);
    int writeResult = i2c_write_byte(sensors->i2cPiHandle, sensors->i2cAdcHandles[adc], newConfig);
    if (writeResult != 0) {
        handleSensorReadFault();
        g_warning("Could not write config to adc: %d - adc:%d, channel:%d", writeResult, adc, channel);
        return;
    }

    g_usleep(ADC_SWITCH_CHANNEL_SLEEP);

    guint8 buf[3];
    int readResult = i2c_read_device(sensors->i2cPiHandle, sensors->i2cAdcHandles[adc], buf, 3);
    if (readResult != 3) {
        handleSensorReadFault();
        g_warning("Could not read adc bytes: %d - adc:%d, channel:%d", readResult, adc, channel);
        return;
    }

    int receivedChannel = getAdcChannelValue(buf[2]);
    if (receivedChannel != channel) {
        handleSensorReadFault();
        g_warning("Channel received %d does not match required - adc:%d, channel:%d", receivedChannel, adc, channel);
        return;
    }

    const guint32 temp = buf[0] << 8 | buf[1];
    const gint32 v = signExtend32(temp, 12);

    if (reading->isFaulty && (v < sensor->base.rawMin + ADC_READING_DEADBAND || v > sensor->base.rawMax - ADC_READING_DEADBAND)) {
        return;
    }

    if (!reading->isFaulty && (v < sensor->base.rawMin || v > sensor->base.rawMax)) {
        handleSensorReadFault();
        g_warning("Raw value %d out of bounds: %d~%d - adc:%d, channel:%d ", v, sensor->base.rawMin, sensor->base.rawMax, adc, channel);
        return;
    }

    SensorReading* vddReading = &sensors->adcReadings[VDD_ADC][VDD_CHANNEL];
    const gdouble vdd = !vddReading->isFaulty ? vddReading->value : VDD_DEFAULT;

    const gdouble value = sensor->convert(v, (int)vdd, sensor->refR);

    setSensorReadingAndWidgets(value, reading, &sensor->base, widgets);
}

void readCanSensor(int canSensorIndex) {
    const CanSensor* sensor = &canSensors[canSensorIndex];
    SensorData* sensors = &workerData.sensors;
    const SensorWidgets* widgets = &sensors->canWidgets[canSensorIndex];
    SensorReading* reading = &sensors->canReadings[canSensorIndex];

    const gdouble value = sensor->getValue();

    if (reading->isFaulty && (value < sensor->base.rawMin + sensor->base.precision || value > sensor->base.rawMax - sensor->base.precision)) {
        return;
    }

    if (!reading->isFaulty && (value < sensor->base.rawMin || value > sensor->base.rawMax)) {
        handleSensorReadFault();
        g_warning("CAN sensor value %f out of bounds: %d~%d - sensor index:%d", value, sensor->base.rawMin, sensor->base.rawMax, canSensorIndex);
        return;
    }

    setSensorReadingAndWidgets(value, reading, &sensor->base, widgets);
}

void setAdcCanFrame() {
    CanFrameState* adcFrame = &workerData.canBus.adcFrame;

    g_mutex_lock(&adcFrame->lock);

    memset(adcFrame->data, 0, CAN_DATA_SIZE);

    SensorReading* transTemp = &workerData.sensors.adcReadings[TRANS_TEMP_ADC][TRANS_TEMP_CHANNEL];
    adcFrame->data[0] = transTemp->isFaulty ? ADC_FRAME_FAULTY_VALUE : (guint8)(transTemp->value + ADC_FRAME_TEMP_OFFSET);

    SensorReading* diffTemp = &workerData.sensors.adcReadings[DIFF_TEMP_ADC][DIFF_TEMP_CHANNEL];
    adcFrame->data[1] = diffTemp->isFaulty ? ADC_FRAME_FAULTY_VALUE : (guint8)(diffTemp->value + ADC_FRAME_TEMP_OFFSET);

    SensorReading* oilTemp = &workerData.sensors.adcReadings[OIL_TEMP_ADC][OIL_TEMP_CHANNEL];
    adcFrame->data[2] = oilTemp->isFaulty ? ADC_FRAME_FAULTY_VALUE : (guint8)(oilTemp->value + ADC_FRAME_TEMP_OFFSET);

    SensorReading* oilPress = &workerData.sensors.adcReadings[OIL_PRESS_ADC][OIL_PRESS_CHANNEL];
    adcFrame->data[3] = oilPress->isFaulty ? ADC_FRAME_FAULTY_VALUE : (guint8)(oilPress->value * 32);

    adcFrame->timestamp = g_get_monotonic_time();
    adcFrame->btWasSent = FALSE;

    g_mutex_unlock(&adcFrame->lock);
}

gpointer sensorWorkerLoop() {
    g_message("Sensor worker starting");

    int i2cPiHandle = pigpio_start(NULL, NULL);
    if (i2cPiHandle < 0)  g_error("Could not connect to pigpiod: %d", i2cPiHandle);
    workerData.sensors.i2cPiHandle = i2cPiHandle;

    int adc0Handle = i2c_open(i2cPiHandle, 1, ADC0_I2C_ADDRESS, 0);
    if (adc0Handle < 0)  g_error("Could not get adc0 handle: %d", adc0Handle);
    workerData.sensors.i2cAdcHandles[0] = adc0Handle;

    int adc1Handle = i2c_open(i2cPiHandle, 1, ADC1_I2C_ADDRESS, 0);
    if (adc1Handle < 0)  g_error("Could not get adc1 handle: %d", adc1Handle);
    workerData.sensors.i2cAdcHandles[1] = adc1Handle;

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

    CanFrameState* adcFrame = &workerData.canBus.adcFrame;
    g_mutex_lock(&adcFrame->lock);
    adcFrame->canId = ADC_FRAME_ID;
    adcFrame->isExtended = FALSE;
    adcFrame->isRemoteRequest = FALSE;
    adcFrame->dataLength = CAN_DATA_SIZE;
    g_mutex_unlock(&adcFrame->lock);

    workerData.isSensorWorkerRunning = TRUE;
    guint shutDownCounter = 0;

    while (workerData.requestShutdown == FALSE) {
        if (workerData.requestMinMaxReset == TRUE) {
            resetReadingsMinMax();
            resetMinMaxLabels();
            workerData.requestMinMaxReset = FALSE;
        }

        gboolean ignOn = gpio_read(i2cPiHandle, IGN_GPIO_PIN);

        SensorReading* pressureReading = &(workerData.sensors.adcReadings)[OIL_PRESS_ADC][OIL_PRESS_CHANNEL];
        const AdcSensor* pressureSensor = &adcSensors[OIL_PRESS_ADC][OIL_PRESS_CHANNEL];

        if (workerData.wasEngineStarted == FALSE &&
            ignOn == TRUE &&
            pressureReading->isFaulty == FALSE &&
            pressureReading->value > pressureSensor->base.alertLow) {
            workerData.wasEngineStarted = TRUE;
        }

        gint32 engineRpm = getEngineRpm();
        gboolean buzzerOn = gpio_read(i2cPiHandle, BUZZER_GPIO_PIN) == TRUE;

        if (engineRpm < OIL_PRESSURE_ALERT_MIN_RPM && buzzerOn) gpio_write(i2cPiHandle, BUZZER_GPIO_PIN, FALSE);

        if (pressureReading->isFaulty == FALSE &&
            pressureReading->value < pressureSensor->base.alertLow &&
            engineRpm > OIL_PRESSURE_ALERT_MIN_RPM &&
            !buzzerOn) {
            gpio_write(i2cPiHandle, BUZZER_GPIO_PIN, TRUE);
        }

#ifndef IS_DEBUG
        shutDownCounter = ignOn == TRUE ? 0 : shutDownCounter + 1;
        if ((workerData.wasEngineStarted == TRUE && shutDownCounter > SHUTDOWN_DELAY_ENGINE_STARTED) ||
            shutDownCounter > SHUTDOWN_DELAY) {
            g_message("Ignition off, requesting system shutdown");
            g_idle_add(shutDown, GINT_TO_POINTER(SystemShutdown));
            break;
        }
#endif

        readAdcSensor(VDD_ADC, VDD_CHANNEL);

        readAdcSensor(OIL_TEMP_ADC, OIL_TEMP_CHANNEL);
        readAdcSensor(OIL_PRESS_ADC, OIL_PRESS_CHANNEL);

        setAdcCanFrame();

        readCanSensor(COOLANT_TEMP_CAN_SENSOR_INDEX);

        g_usleep(SENSOR_WORKER_LOOP_INTERVAL);
    }

    gpio_write(i2cPiHandle, BUZZER_GPIO_PIN, FALSE);
    i2c_close(i2cPiHandle, adc0Handle);
    i2c_close(i2cPiHandle, adc1Handle);
    pigpio_stop(i2cPiHandle);

    g_message("Sensor worker shutting down");
    workerData.isSensorWorkerRunning = FALSE;

    return NULL;
}