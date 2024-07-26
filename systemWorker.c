#include <pigpiod_if2.h>

#include "dataContracts.h"
#include "sensorProps.c"
#include "canBusProps.c"
#include "ui.c"

#define SYSTEM_WORKER_LOOP_INTERVAL_US 50000
#define SHUTDOWN_DELAY_US 20000000
#define SHUTDOWN_DELAY_CYCLES (SHUTDOWN_DELAY_US / SYSTEM_WORKER_LOOP_INTERVAL_US)
#define SHUTDOWN_DELAY_AFTER_ENGINE_STARTED_US 2000000
#define SHUTDOWN_DELAY_AFTER_ENGINE_STARTED_CYCLES (SHUTDOWN_DELAY_AFTER_ENGINE_STARTED_US / SYSTEM_WORKER_LOOP_INTERVAL_US)

#define IGN_GPIO_PIN 22
#define TRANS_PUMP_GPIO_PIN 23
#define BUZZER_GPIO_PIN 27

#define ENGINE_RUNNING_OIL_PRESSURE 0.5
#define ENGINE_RUNNING_RPM 100
#define OIL_PRESSURE_BUZZER_ON_RPM 1300

#define BUZZER_CHIRP_COUNT_PER_CYCLE 5
#define BUZZER_CHIRP_ON_US 100000
#define BUZZER_CHIRP_OFF_US 100000
#define BUZZER_CHIRP_CYCLE_US 4000000

#define TRANS_PUMP_ON_TEMP_C 95.0
#define TRANS_PUMP_ON_MAX_TIME_US 20000000
#define TRANS_PUMP_ON_MAX_CYCLES (TRANS_PUMP_ON_MAX_TIME_US / SYSTEM_WORKER_LOOP_INTERVAL_US)
#define TRANS_PUMP_OFF_TEMP_C 50.0
#define TRANS_PUMP_OFF_MAX_TIME_US 300000000
#define TRANS_PUMP_OFF_MAX_CYCLES (TRANS_PUMP_OFF_MAX_TIME_US / SYSTEM_WORKER_LOOP_INTERVAL_US)
#define TRANS_PUMP_OFF_MIN_TIME_US 2000000
#define TRANS_PUMP_OFF_MIN_CYCLES (TRANS_PUMP_OFF_MIN_TIME_US / SYSTEM_WORKER_LOOP_INTERVAL_US)

gboolean getShouldBuzzerChirp() {
    for (guint i = 0; i < ADC_COUNT; i++) for (guint j = 0; j < ADC_CHANNEL_COUNT; j++)
    {
        SensorReading* reading = &appData.sensors.adcReadings[i][j];
        SensorBase* sensor = &adcSensors[i][j].base;
        if (reading->state < StateNotifyLow || reading->state > StateNotifyHigh) return TRUE;
    }

    for (guint i = 0; i < CAN_SENSORS_COUNT; i++)
    {
        SensorReading* reading = &appData.sensors.canReadings[i];
        SensorBase* sensor = &canSensors[i].base;
        if (reading->state < StateNotifyLow || reading->state > StateNotifyHigh) return TRUE;
    }

    return FALSE;
}

void setBuzzer(gdouble engineRpm, gdouble oilPressure)
{
    static gboolean wasBuzzerOn = FALSE;
    static guint64 buzzerLastToggleUs = 0;
    static guint8 buzzerChirpCount = BUZZER_CHIRP_COUNT_PER_CYCLE;

    gboolean isBuzzerOn = gpio_read(appData.system.pigpioHandle, BUZZER_GPIO_PIN) == TRUE;

    gboolean shouldBuzzerBeOn =
        oilPressure < adcSensors[OIL_PRESS_ADC][OIL_PRESS_CHANNEL].base.alertLow &&
        engineRpm > OIL_PRESSURE_BUZZER_ON_RPM;

    if (shouldBuzzerBeOn) {
        if (!isBuzzerOn) gpio_write(appData.system.pigpioHandle, BUZZER_GPIO_PIN, TRUE);
        if (!wasBuzzerOn) wasBuzzerOn = TRUE;
        return;
    }

    if (!shouldBuzzerBeOn && wasBuzzerOn) {
        gpio_write(appData.system.pigpioHandle, BUZZER_GPIO_PIN, FALSE);
        wasBuzzerOn = FALSE;
        buzzerLastToggleUs = g_get_monotonic_time();
        buzzerChirpCount = BUZZER_CHIRP_COUNT_PER_CYCLE;
        return;
    }

    gboolean shouldBuzzerChirp = getShouldBuzzerChirp();
    guint64 timeSinceLastToggleUs = g_get_monotonic_time() - buzzerLastToggleUs;

    if (shouldBuzzerChirp &&
        buzzerChirpCount == BUZZER_CHIRP_COUNT_PER_CYCLE &&
        timeSinceLastToggleUs > BUZZER_CHIRP_CYCLE_US) {
        buzzerChirpCount = 0;
    }

    if (buzzerChirpCount >= BUZZER_CHIRP_COUNT_PER_CYCLE) return;

    if (!isBuzzerOn && timeSinceLastToggleUs > BUZZER_CHIRP_OFF_US) {
        gpio_write(appData.system.pigpioHandle, BUZZER_GPIO_PIN, TRUE);
        buzzerLastToggleUs = g_get_monotonic_time();
    }

    if (isBuzzerOn && timeSinceLastToggleUs > BUZZER_CHIRP_ON_US) {
        gpio_write(appData.system.pigpioHandle, BUZZER_GPIO_PIN, FALSE);
        buzzerLastToggleUs = g_get_monotonic_time();
        buzzerChirpCount++;
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
    gdouble transTemp = getAdcSensorValue(TRANS_TEMP_ADC, TRANS_TEMP_CHANNEL);

    if (isTransPumpOn && !appData.system.isEngineRunning) {
        setTransPumpState(FALSE);
        return;
    }

    if (!appData.system.isEngineRunning) return;

    if (!isTransPumpOn && transPumpCounter < TRANS_PUMP_OFF_MIN_CYCLES) { return; }

    if (!isTransPumpOn && transTemp >= TRANS_PUMP_ON_TEMP_C) {
        setTransPumpState(TRUE);
        return;
    }

    if (isTransPumpOn && transTemp >= TRANS_PUMP_ON_TEMP_C) {
        return;
    }

    if (!isTransPumpOn && transTemp >= TRANS_PUMP_OFF_TEMP_C && transPumpCounter > TRANS_PUMP_OFF_MAX_CYCLES) {
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

        gdouble engineRpm = getEngineRpm();
        gdouble oilPressure = getAdcSensorValue(OIL_PRESS_ADC, OIL_PRESS_CHANNEL);

        appData.system.isEngineRunning =
            appData.system.isIgnOn &&
            (oilPressure > ENGINE_RUNNING_OIL_PRESSURE || engineRpm > ENGINE_RUNNING_RPM);

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

        setBuzzer(engineRpm, oilPressure);

        switchTransPumpOnOff();

#ifndef IS_DEBUG
        shutDownCounter = appData.system.isIgnOn ? 0 : shutDownCounter + 1;
        if ((appData.system.wasEngineStarted && shutDownCounter > SHUTDOWN_DELAY_AFTER_ENGINE_STARTED_CYCLES) ||
            shutDownCounter > SHUTDOWN_DELAY_CYCLES) {
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
