#include <gtk/gtk.h>
#include <pigpiod_if2.h>
#include <math.h>

#include "dataContracts.h"
#include "appData.c"
#include "helpers.c"
#include "sensorProps.c"
#include "canBusProps.c"
#include "ui.c"

#define SENSOR_WORKER_LOOP_INTERVAL_US 25000
#define ADC_SWITCH_CHANNEL_SLEEP_US 5000

#define FAULTY_READING_LABEL "--"
#define ADC_READING_DEADBAND 10

#define ADC0_I2C_ADDRESS 0x6a
#define ADC1_I2C_ADDRESS 0x6c
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
            const SensorBase* sensor = &adcSensors[i][j].base;
            SensorReading* reading = &appData.sensors.adcReadings[i][j];
            reading->value = sensor->defaultValue;
            reading->errorCount = 1;
        }
    }
    for (int i = 0; i < CAN_SENSORS_COUNT; i++) {
        const SensorBase* sensor = &canSensors[i].base;
        SensorReading* reading = &appData.sensors.canReadings[i];
        reading->value = sensor->defaultValue;
        reading->errorCount = 1;
    }
};

void resetReadingsMinMax() {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            appData.sensors.adcReadings[i][j].min = G_MAXDOUBLE;
            appData.sensors.adcReadings[i][j].max = -G_MAXDOUBLE;
        }
    }

    for (int i = 0; i < CAN_SENSORS_COUNT; i++) {
        appData.sensors.canReadings[i].min = G_MAXDOUBLE;
        appData.sensors.canReadings[i].max = -G_MAXDOUBLE;
    }
};

void setSingleSensorWidgets(const SensorBase* sensor, SensorWidgets* widgets) {
    GtkBuilder* builder = appData.builder;
    if (sensor->labelId != NULL) widgets->label = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelId));
    if (sensor->frameId != NULL) widgets->frame = GTK_FRAME(gtk_builder_get_object(builder, sensor->frameId));
    if (sensor->labelMinId != NULL) widgets->labelMin = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelMinId));
    if (sensor->labelMaxId != NULL) widgets->labelMax = GTK_LABEL(gtk_builder_get_object(builder, sensor->labelMaxId));
}

void setWidgets() {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            const SensorBase* sensor = &adcSensors[i][j].base;
            SensorWidgets* widgets = &appData.sensors.adcWidgets[i][j];
            setSingleSensorWidgets(sensor, widgets);
        }
    }
    for (int i = 0; i < CAN_SENSORS_COUNT; i++) {
        const SensorBase* sensor = &canSensors[i].base;
        SensorWidgets* widgets = &appData.sensors.canWidgets[i];
        setSingleSensorWidgets(sensor, widgets);
    }
};

void resetMinMaxLabels() {
    for (int i = 0; i < ADC_COUNT; i++) {
        for (int j = 0; j < ADC_CHANNEL_COUNT; j++) {
            const SensorWidgets* widgets = &appData.sensors.adcWidgets[i][j];
            setLabel(widgets->labelMin, FAULTY_READING_LABEL, 0);
            setLabel(widgets->labelMax, FAULTY_READING_LABEL, 0);
        }
    }
    for (int i = 0; i < CAN_SENSORS_COUNT; i++) {
        const SensorWidgets* widgets = &appData.sensors.canWidgets[i];
        setLabel(widgets->labelMin, FAULTY_READING_LABEL, 0);
        setLabel(widgets->labelMax, FAULTY_READING_LABEL, 0);

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

#define handleSensorReadFault(_allowSomeErrors)\
    sensors->errorCount++;\
    reading->errorCount++;\
    if (!_allowSomeErrors || (reading->errorCount > SENSOR_WARNING_ERROR_COUNT && reading->errorCount < SENSOR_CRITICAL_ERROR_COUNT)) {\
        setLabel(widgets->label, FAULTY_READING_LABEL, 0);\
        setFrame(widgets->frame, StateNormal);\
    }\

void setSensorReadingAndWidgets(
    gdouble value,
    SensorReading* reading,
    const SensorBase* sensor,
    const SensorWidgets* widgets) {

    if (!reading->errorCount &&
        reading->min != G_MAXDOUBLE &&
        reading->max != -G_MAXDOUBLE &&
        value < reading->value + sensor->precision &&
        value > reading->value - sensor->precision
        ) return;

    gboolean wasFaulty = !!reading->errorCount;

    reading->errorCount = 0;
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
    if (wasFaulty || state != reading->state) {
        reading->state = state;
        setFrame(widgets->frame, state);
    }
}

gboolean readAdc(int adc, int channel, AdcPga pga, guint32* result) {
    SensorData* sensors = &appData.sensors;
    const SensorWidgets* widgets = &sensors->adcWidgets[adc][channel];
    SensorReading* reading = &sensors->adcReadings[adc][channel];

    sensors->requestCount++;

    guint8 adcConfig = ADC_DEFAULT_CONFIG | getAdcChannelBits(channel) | pga;
    int writeResult = i2c_write_byte(sensors->i2cPiHandle, sensors->i2cAdcHandles[adc], adcConfig);
    if (writeResult != 0) {
        handleSensorReadFault(TRUE);
        g_warning("Could not write config to ADC: %d - ADC:%d, channel:%d", writeResult, adc, channel);
        return FALSE;
    }

    g_usleep(ADC_SWITCH_CHANNEL_SLEEP_US);

    guint8 buf[3];
    int readResult = i2c_read_device(sensors->i2cPiHandle, sensors->i2cAdcHandles[adc], buf, 3);
    if (readResult != 3) {
        handleSensorReadFault(TRUE);
        g_warning("Could not read ADC bytes: %d - ADC:%d, channel:%d", readResult, adc, channel);
        return FALSE;
    }

    int receivedChannel = getAdcChannelValue(buf[2]);
    if (receivedChannel != channel) {
        handleSensorReadFault(TRUE);
        g_warning("Received ADC channel:%d does not match required - ADC:%d, channel:%d", receivedChannel, adc, channel);
        return FALSE;
    }

    *result = (buf[0] << 8 | buf[1]) / AdcPgaMultipliers[pga];

    return TRUE;
}

void readAdcSensor(int adc, int channel) {
    SensorData* sensors = &appData.sensors;
    const AdcSensor* sensor = &adcSensors[adc][channel];
    SensorReading* reading = &sensors->adcReadings[adc][channel];
    const SensorWidgets* widgets = &sensors->adcWidgets[adc][channel];

    AdcPga pga = sensor->pga;
    guint32 digits;

    if (pga == AdcPgaAdaptive) {
        pga = AdcPgaX1;
        if (!readAdc(adc, channel, pga, &digits)) { return; }

        if (digits <= AdcPgaLimits[AdcPgaX8]) {
            pga = AdcPgaX8;
            if (!readAdc(adc, channel, pga, &digits)) { return; }
        }

        if (digits > AdcPgaLimits[AdcPgaX8] && digits <= AdcPgaLimits[AdcPgaX4]) {
            pga = AdcPgaX4;
            if (!readAdc(adc, channel, pga, &digits)) { return; }
        }

        if (digits > AdcPgaLimits[AdcPgaX4] && digits <= AdcPgaLimits[AdcPgaX2]) {
            pga = AdcPgaX2;
            if (!readAdc(adc, channel, pga, &digits)) { return; }
        }
    }
    else {
        if (!readAdc(adc, channel, pga, &digits)) { return; }
    }


    const gint32 raw = signExtend32(digits, ADC_BIT_RESOLUTION);

    if (reading->errorCount &&
        (raw < sensor->base.rawMin + ADC_READING_DEADBAND || raw > sensor->base.rawMax - ADC_READING_DEADBAND)) {
        reading->errorCount++;
        return;
    }

    if (!reading->errorCount && (raw < sensor->base.rawMin || raw > sensor->base.rawMax)) {
        handleSensorReadFault(FALSE);
        g_warning(
            "Raw value %d out of bounds: %d~%d - adc:%d, channel:%d ",
            raw, sensor->base.rawMin, sensor->base.rawMax, adc, channel);
        return;
    }

    SensorReading* vddReading = &sensors->adcReadings[VDD_ADC][VDD_CHANNEL];
    const SensorBase* vddSensor = &adcSensors[VDD_ADC][VDD_CHANNEL].base;
    const gdouble vdd = !vddReading->errorCount ? vddReading->value : vddSensor->defaultValue;

    const gdouble value = sensor->convert(raw, (int)vdd, sensor->refR);

    setSensorReadingAndWidgets(value, reading, &sensor->base, widgets);
}

void readCanSensor(guint canSensorIndex) {
    if (!appData.isCanBusWorkerRunning || appData.canBusRestartRequested) return;

    const CanSensor* sensor = &canSensors[canSensorIndex];
    SensorData* sensors = &appData.sensors;
    const SensorWidgets* widgets = &sensors->canWidgets[canSensorIndex];
    SensorReading* reading = &sensors->canReadings[canSensorIndex];

    sensors->requestCount++;

    if (!appData.system.isIgnOn) {
        if (!reading->errorCount) handleSensorReadFault(FALSE);
        return;
    }

    const gdouble value = sensor->getValue();

    if (reading->errorCount &&
        (value < sensor->base.rawMin + sensor->base.precision || value > sensor->base.rawMax - sensor->base.precision)) {
        reading->errorCount++;
        return;
    }

    if (!reading->errorCount && (value < sensor->base.rawMin || value > sensor->base.rawMax)) {
        handleSensorReadFault(FALSE);
        g_warning(
            "CAN sensor value %f out of bounds: %d~%d - sensor index:%d",
            value, sensor->base.rawMin, sensor->base.rawMax, canSensorIndex);
        return;
    }

    setSensorReadingAndWidgets(value, reading, &sensor->base, widgets);
}

void setAdcCanFrame() {
    CanFrameState* adcFrame = &appData.canBus.adcFrame;

    g_mutex_lock(&adcFrame->lock);

    memset(adcFrame->data, 0, CAN_DATA_SIZE);

    SensorReading* transTemp = &appData.sensors.adcReadings[TRANS_TEMP_ADC][TRANS_TEMP_CHANNEL];
    adcFrame->data[0] = transTemp->errorCount ? ADC_FRAME_FAULTY_VALUE : (guint8)(round(transTemp->value) + ADC_FRAME_TEMP_OFFSET);

    SensorReading* diffTemp = &appData.sensors.adcReadings[DIFF_TEMP_ADC][DIFF_TEMP_CHANNEL];
    adcFrame->data[1] = diffTemp->errorCount ? ADC_FRAME_FAULTY_VALUE : (guint8)(round(diffTemp->value) + ADC_FRAME_TEMP_OFFSET);

    SensorReading* oilTemp = &appData.sensors.adcReadings[OIL_TEMP_ADC][OIL_TEMP_CHANNEL];
    adcFrame->data[2] = oilTemp->errorCount ? ADC_FRAME_FAULTY_VALUE : (guint8)(round(oilTemp->value) + ADC_FRAME_TEMP_OFFSET);

    SensorReading* oilPress = &appData.sensors.adcReadings[OIL_PRESS_ADC][OIL_PRESS_CHANNEL];
    adcFrame->data[3] = oilPress->errorCount ? ADC_FRAME_FAULTY_VALUE : (guint8)(round(oilPress->value * ADC_FRAME_PRESS_FACTOR));

    SensorReading* rotorTemp = &appData.sensors.adcReadings[ROTOR_TEMP_ADC][ROTOR_TEMP_CHANNEL];
    adcFrame->data[4] = rotorTemp->errorCount ? ADC_FRAME_FAULTY_VALUE : (guint8)(round(rotorTemp->value * ADC_FRAME_ROTOR_TEMP_FACTOR));

    SensorReading* caliperTemp = &appData.sensors.adcReadings[CALIPER_TEMP_ADC][CALIPER_TEMP_CHANNEL];
    adcFrame->data[5] = caliperTemp->errorCount ? ADC_FRAME_FAULTY_VALUE : (guint8)(round(caliperTemp->value) + ADC_FRAME_CALIPER_TEMP_OFFSET);

    adcFrame->timestamp = g_get_monotonic_time();
    adcFrame->btWasSent = FALSE;

    g_mutex_unlock(&adcFrame->lock);
}

//-------------------------------------------------------------------------------------------------------------

gpointer sensorWorkerLoop() {
    g_message("Sensor worker starting");

    gint i2cPiHandle = pigpio_start(NULL, NULL);
    if (i2cPiHandle < 0)  g_error("Could not connect to pigpiod: %d", i2cPiHandle);
    appData.sensors.i2cPiHandle = i2cPiHandle;

    gint adc0Handle = i2c_open(i2cPiHandle, 1, ADC0_I2C_ADDRESS, 0);
    if (adc0Handle < 0)  g_error("Could not get adc0 handle: %d", adc0Handle);
    appData.sensors.i2cAdcHandles[0] = adc0Handle;

    gint adc1Handle = i2c_open(i2cPiHandle, 1, ADC1_I2C_ADDRESS, 0);
    if (adc1Handle < 0)  g_error("Could not get adc1 handle: %d", adc1Handle);
    appData.sensors.i2cAdcHandles[1] = adc1Handle;

    resetReadingsValues();
    resetReadingsMinMax();
    setWidgets();

    CanFrameState* adcFrame = &appData.canBus.adcFrame;
    g_mutex_lock(&adcFrame->lock);
    adcFrame->canId = ADC_FRAME_ID;
    adcFrame->isExtended = FALSE;
    adcFrame->isRemoteRequest = FALSE;
    adcFrame->dataLength = CAN_DATA_SIZE;
    g_mutex_unlock(&adcFrame->lock);

    appData.isSensorWorkerRunning = TRUE;

    while (!appData.shutdownRequested) {
        if (appData.minMaxResetRequested) {
            resetReadingsMinMax();
            resetMinMaxLabels();
            appData.minMaxResetRequested = FALSE;
        }

        readAdcSensor(VDD_ADC, VDD_CHANNEL);

        readAdcSensor(TRANS_TEMP_ADC, TRANS_TEMP_CHANNEL);
        readAdcSensor(DIFF_TEMP_ADC, DIFF_TEMP_CHANNEL);
        readAdcSensor(OIL_TEMP_ADC, OIL_TEMP_CHANNEL);
        readAdcSensor(OIL_PRESS_ADC, OIL_PRESS_CHANNEL);
        readAdcSensor(ROTOR_TEMP_ADC, ROTOR_TEMP_CHANNEL);
        readAdcSensor(CALIPER_TEMP_ADC, CALIPER_TEMP_CHANNEL);

        setAdcCanFrame();

        readCanSensor(COOLANT_TEMP_CAN_SENSOR_INDEX);

        g_usleep(SENSOR_WORKER_LOOP_INTERVAL_US);
    }

    appData.isSensorWorkerRunning = FALSE;

    i2c_close(i2cPiHandle, adc0Handle);
    i2c_close(i2cPiHandle, adc1Handle);
    pigpio_stop(i2cPiHandle);

    g_message(
        "ADC sensors requests:%d, errors:%d, rate:%.4f",
        appData.sensors.requestCount,
        appData.sensors.errorCount,
        (float)appData.sensors.errorCount / appData.sensors.requestCount);

    g_message("Sensor worker shutting down");

    return NULL;
}