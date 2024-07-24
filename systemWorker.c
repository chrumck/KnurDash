#include <pigpiod_if2.h>

#include "dataContracts.h"
#include "sensorProps.c"
#include "ui.c"

#define SYSTEM_WORKER_LOOP_INTERVAL_US 25000
#define SHUTDOWN_DELAY 600
#define SHUTDOWN_DELAY_AFTER_ENGINE_STARTED 30

#define IGN_GPIO_PIN 22
#define TRANS_PUMP_GPIO_PIN 23
#define BUZZER_GPIO_PIN 27

#define OIL_PRESSURE_MIN 0.5
#define OIL_PRESSURE_ALERT_MIN_RPM 1300
#define ENGINE_RPM_MIN 100

#define TRANS_PUMP_ON_TEMP_C 95.0
#define TRANS_PUMP_ON_MAX_TIME_US 20000000
#define TRANS_PUMP_ON_MAX_CYCLES (TRANS_PUMP_ON_MAX_TIME_US / SYSTEM_WORKER_LOOP_INTERVAL_US)
#define TRANS_PUMP_OFF_TEMP_C 50.0
#define TRANS_PUMP_OFF_MAX_TIME_US 300000000
#define TRANS_PUMP_OFF_MAX_CYCLES (TRANS_PUMP_OFF_MAX_TIME_US / SYSTEM_WORKER_LOOP_INTERVAL_US)
#define TRANS_PUMP_OFF_MIN_TIME_US 2000000
#define TRANS_PUMP_OFF_MIN_CYCLES (TRANS_PUMP_OFF_MIN_TIME_US / SYSTEM_WORKER_LOOP_INTERVAL_US)

gdouble getAdcSensorValue(gint adc, gint channel) {
    SensorReading* reading = &(appData.sensors.adcReadings)[adc][channel];
    const AdcSensor* sensor = &adcSensors[adc][channel];

    return reading->errorCount < SENSOR_WARNING_ERROR_COUNT ?
        reading->value : sensor->base.defaultValue;
}

void setBuzzer(gdouble pressureValue, gdouble engineRpm)
{
    gboolean isBuzzerOn = gpio_read(appData.system.pigpioHandle, BUZZER_GPIO_PIN) == TRUE;
    const AdcSensor* pressureSensor = &adcSensors[OIL_PRESS_ADC][OIL_PRESS_CHANNEL];

    if (isBuzzerOn && (pressureValue >= pressureSensor->base.alertLow || engineRpm < OIL_PRESSURE_ALERT_MIN_RPM)) {
        gpio_write(appData.system.pigpioHandle, BUZZER_GPIO_PIN, FALSE);
    }

    if (!isBuzzerOn &&
        pressureValue < pressureSensor->base.alertLow &&
        engineRpm > OIL_PRESSURE_ALERT_MIN_RPM) {
        gpio_write(appData.system.pigpioHandle, BUZZER_GPIO_PIN, TRUE);
    }
}

#define setTransPumpState(_state)\
        gpio_write(appData.system.pigpioHandle, TRANS_PUMP_GPIO_PIN, _state);\
        transPumpCounter = 0;\
        g_idle_add(setTransPumpStatus, GUINT_TO_POINTER(_state));

void switchTransPumpOnOff() {
    static guint transPumpCounter = 0;

    transPumpCounter++;

    gboolean isTransPumpOn = gpio_read(appData.system.pigpioHandle, TRANS_PUMP_GPIO_PIN) == TRUE;
    gdouble transTempValue = getAdcSensorValue(TRANS_TEMP_ADC, TRANS_TEMP_CHANNEL);

    if (isTransPumpOn && !appData.system.isEngineRunning) {
        setTransPumpState(FALSE);
        return;
    }

    if (!appData.system.isEngineRunning) return;

    if (!isTransPumpOn && transPumpCounter < TRANS_PUMP_OFF_MIN_CYCLES) { return; }

    if (!isTransPumpOn && transTempValue >= TRANS_PUMP_ON_TEMP_C) {
        setTransPumpState(TRUE);
        return;
    }

    if (isTransPumpOn && transTempValue >= TRANS_PUMP_ON_TEMP_C) {
        return;
    }

    if (!isTransPumpOn && transTempValue >= TRANS_PUMP_OFF_TEMP_C && transPumpCounter > TRANS_PUMP_OFF_MAX_CYCLES) {
        setTransPumpState(TRUE);
        return;
    }

    if (isTransPumpOn && transPumpCounter > TRANS_PUMP_ON_MAX_CYCLES) {
        setTransPumpState(FALSE)
    }
}

gpointer systemWorkerLoop() {
    while (!appData.shutdownRequested && !appData.isSensorWorkerRunning) { g_usleep(SYSTEM_WORKER_LOOP_INTERVAL_US); }

    if (appData.shutdownRequested || !appData.isSensorWorkerRunning) { return NULL; }

    g_message("System worker starting");

    gint pigpioHandle = pigpio_start(NULL, NULL);
    if (pigpioHandle < 0) {
        logError("Could not connect to pigpiod: %d", pigpioHandle);
        return NULL;
    }

    appData.system.pigpioHandle = pigpioHandle;

    gint gpioResult = 0;
    gpioResult = set_mode(pigpioHandle, IGN_GPIO_PIN, PI_INPUT);
    if (gpioResult != 0) {
        logError("Could not set GPIO pin mode for IGN_IN: %d", gpioResult);
        return NULL;
    }

    gpioResult = set_pull_up_down(pigpioHandle, IGN_GPIO_PIN, PI_PUD_DOWN);
    if (gpioResult != 0) {
        logError("Could not set GPIO pin pulldown for IGN_IN: %d", gpioResult);
        return NULL;
    }

    gpioResult = set_mode(pigpioHandle, BUZZER_GPIO_PIN, PI_OUTPUT);
    if (gpioResult != 0) {
        logError("Could not set GPIO pin mode for buzzer: %d", gpioResult);
        return NULL;
    }

    gpioResult = set_pull_up_down(pigpioHandle, BUZZER_GPIO_PIN, PI_PUD_OFF);
    if (gpioResult != 0) {
        logError("Could not set GPIO pin pulldown for buzzer: %d", gpioResult);
        return NULL;
    }

    gpioResult = set_mode(pigpioHandle, TRANS_PUMP_GPIO_PIN, PI_OUTPUT);
    if (gpioResult != 0) {
        logError("Could not set GPIO pin mode for trans pump: %d", gpioResult);
        return NULL;
    }

    gpioResult = set_pull_up_down(pigpioHandle, TRANS_PUMP_GPIO_PIN, PI_PUD_DOWN);
    if (gpioResult != 0) {
        logError("Could not set GPIO pin pulldown for trans pump: %d", gpioResult);
        return NULL;
    }

    gpioResult = gpio_write(pigpioHandle, TRANS_PUMP_GPIO_PIN, FALSE);
    if (gpioResult != 0) {
        logError("Could not set GPIO pin state for trans pump: %d", gpioResult);
        return NULL;
    }

    guint shutDownCounter = 0;

    appData.isSystemWorkerRunning = TRUE;
    g_message("System worker started");

    while (!appData.shutdownRequested && appData.isSensorWorkerRunning) {
        float errorRate = (float)appData.sensors.errorCount / appData.sensors.requestCount;
        if (errorRate > MAX_REQUEST_ERROR_RATE &&
            g_get_monotonic_time() - appData.startupTimestamp > MIN_APP_RUNNING_TIME_US) {
            g_warning("ADC sensors excessive error rate:%2f, shutting down app", errorRate);
            g_idle_add(shutDown, GUINT_TO_POINTER(AppShutdownDueToErrors));
            break;
        }

        appData.system.isIgnOn = gpio_read(pigpioHandle, IGN_GPIO_PIN);

        gdouble pressureValue = getAdcSensorValue(OIL_PRESS_ADC, OIL_PRESS_CHANNEL);
        gdouble engineRpm = getEngineRpm();

        appData.system.isEngineRunning = appData.system.isIgnOn &&
            (pressureValue > OIL_PRESSURE_MIN || engineRpm > ENGINE_RPM_MIN);

        if (!appData.system.wasEngineStarted && appData.system.isEngineRunning) { appData.system.wasEngineStarted = TRUE; }

        guint32* coolantTempReadingErrorCount = &(appData.sensors.canReadings[COOLANT_TEMP_CAN_SENSOR_INDEX].errorCount);
        if (ENABLE_CANBUS &&
            appData.system.isIgnOn &&
            appData.system.wasEngineStarted &&
            !appData.canBusRestartRequested &&
            *coolantTempReadingErrorCount > SENSOR_CRITICAL_ERROR_COUNT) {
            g_warning("Coolant temp excessive error count:%d, requesting CAN restart", *coolantTempReadingErrorCount);
            *coolantTempReadingErrorCount = 1;
            appData.canBusRestartRequested = TRUE;
        }

        setBuzzer(pressureValue, engineRpm);

        switchTransPumpOnOff();

#ifndef IS_DEBUG
        shutDownCounter = appData.system.isIgnOn ? 0 : shutDownCounter + 1;
        if ((appData.system.wasEngineStarted && shutDownCounter > SHUTDOWN_DELAY_AFTER_ENGINE_STARTED) ||
            shutDownCounter > SHUTDOWN_DELAY) {
            g_message("Ignition off, requesting system shutdown");
            g_idle_add(shutDown, GUINT_TO_POINTER(SystemShutdown));
            break;
        }
#endif
        g_usleep(SYSTEM_WORKER_LOOP_INTERVAL_US);
    }

    appData.isSystemWorkerRunning = FALSE;

    gpio_write(pigpioHandle, BUZZER_GPIO_PIN, FALSE);
    gpio_write(pigpioHandle, TRANS_PUMP_GPIO_PIN, FALSE);

    pigpio_stop(pigpioHandle);

    g_message("System worker shutting down");

    return NULL;
}
