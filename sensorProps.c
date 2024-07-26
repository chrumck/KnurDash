#pragma once

#include <gtk/gtk.h>
#include <math.h>

#include "dataContracts.h"

#define ADC_DEFAULT_CONFIG 0x10
#define ADC_BIT_RESOLUTION 12

#define VDD_DEFAULT 3350
#define VDD_ADC 1
#define VDD_CHANNEL 3

#define TRANS_TEMP_ADC 0
#define TRANS_TEMP_CHANNEL 0
#define DIFF_TEMP_ADC 0
#define DIFF_TEMP_CHANNEL 1
#define OIL_TEMP_ADC 0
#define OIL_TEMP_CHANNEL 2
#define OIL_PRESS_ADC 0
#define OIL_PRESS_CHANNEL 3

#define ROTOR_TEMP_ADC 1
#define ROTOR_TEMP_CHANNEL 1
#define CALIPER_TEMP_ADC 1
#define CALIPER_TEMP_CHANNEL 2

#define TEMP_SENSOR_RAW_MIN 30
#define TEMP_SENSOR_RAW_MAX 2030

#define ROTOR_TEMP_SENSOR_RAW_MIN 226 // 50 degC
#define ROTOR_TEMP_SENSOR_RAW_MAX 1000

#define CALIPER_TEMP_SENSOR_RAW_MIN 15
#define CALIPER_TEMP_SENSOR_RAW_MAX 2040

#define PRESS_SENSOR_RAW_MIN 30
#define PRESS_SENSOR_RAW_MAX 1000

#define VDD_RAW_MIN 1550
#define VDD_RAW_MAX 1750

#define TEMP_A 276.3
#define TEMP_B -39.95
#define TEMP_C 0.09136

#define CALIPER_TEMP_A 761.89
#define CALIPER_TEMP_B -137.70
#define CALIPER_TEMP_C 9.5068
#define CALIPER_TEMP_D -0.27223

#define PRESS_A -0.2967
#define PRESS_B 0.02529
#define PRESS_C 0.0000350

#define ROTOR_TEMP_A -250
#define ROTOR_TEMP_B 62.5

gdouble convertTemp(gint32 sensorV, gint32 driveV, gint32 refR) {
    const gdouble sensorR = (gdouble)sensorV * refR / (driveV - sensorV);
    const gdouble logSensorR = log(sensorR);
    return TEMP_A + (TEMP_B * logSensorR) + (TEMP_C * pow(logSensorR, 3));
}

gdouble convertOilPress(gint32 sensorV, gint32 driveV, gint32 refR) {
    const gdouble sensorR = (gdouble)sensorV * refR / (driveV - sensorV);
    const gdouble value = PRESS_A + (PRESS_B * sensorR) + (PRESS_C * pow(sensorR, 2));
    return value < 0 ? 0 : value;
}

gdouble convertVdd(gint32 sensorV, gint32 driveV, gint32 refR) {
    return sensorV * 2;
}

gdouble convertRotorTemp(gint32 sensorV, gint32 driveV, gint32 refR) {
    const gdouble sensorI = (gdouble)sensorV / ((gdouble)refR / 10);
    const double value = ROTOR_TEMP_A + (ROTOR_TEMP_B * sensorI);
    return value < 0 ? 0 : value;
}

gdouble convertCaliperTemp(gint32 sensorV, gint32 driveV, gint32 refR) {
    const gdouble sensorR = (gdouble)sensorV * refR / (driveV - sensorV);
    const gdouble logSensorR = log(sensorR);
    return CALIPER_TEMP_A + CALIPER_TEMP_B * logSensorR + CALIPER_TEMP_C * pow(logSensorR, 2) + CALIPER_TEMP_D * pow(logSensorR, 3);
}

gdouble getAdcSensorValue(gint adc, gint channel) {
    SensorReading* reading = &(appData.sensors.adcReadings)[adc][channel];
    const AdcSensor* sensor = &adcSensors[adc][channel];

    return reading->errorCount < SENSOR_WARNING_ERROR_COUNT ?
        reading->value : sensor->base.defaultValue;
}

const AdcSensor adcSensors[ADC_COUNT][ADC_CHANNEL_COUNT] = {
    {
        {
            .base = {
                .labelId = "transTemp", .frameId = "transTempFrame", .labelMinId = "transTempMin", .labelMaxId = "transTempMax",
                .alertLow = -25, .warningLow = -25, .notifyLow = -25,
                .notifyHigh = 130, .warningHigh = 130, .alertHigh = 140,
                .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX,
                .defaultValue = 10.0, .format = "%.0f" , .precision = 0.3,
            },
             .pga = AdcPgaX1, .refR = 2012, .convert = convertTemp,
        },
        {
            .base = {
                .labelId = "diffTemp", .frameId = "diffTempFrame", .labelMinId = "diffTempMin", .labelMaxId = "diffTempMax",
                .alertLow = -25, .warningLow = -25, .notifyLow = -25,
                .notifyHigh = 135, .warningHigh = 135, .alertHigh = 150,
                .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX,
                .defaultValue = 10.0, .format = "%.0f" , .precision = 0.3,
            },
            .pga = AdcPgaX1, .refR = 2006, .convert = convertTemp,
        },
        {
            .base = {
                .labelId = "oilTemp", .frameId = "oilTempFrame", .labelMinId = "oilTempMin", .labelMaxId = "oilTempMax",
                .alertLow = -30, .warningLow = -30, .notifyLow = 80,
                .notifyHigh = 125, .warningHigh = 125, .alertHigh = 135,
                .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX,
                .defaultValue = 10.0, .format = "%.0f" , .precision = 0.3,
            },
            .pga = AdcPgaX1, .refR = 1992, .convert = convertTemp,
        },
        {
            .base = {
                .labelId = "oilPress", .frameId = "oilPressFrame", .labelMinId = "oilPressMin", .labelMaxId = "oilPressMax",
                .alertLow = 0.7, .warningLow = 1.0, .notifyLow = 1.0,
                .notifyHigh = 5.5, .warningHigh = 5.5, .alertHigh = 6.0,
                .rawMin = PRESS_SENSOR_RAW_MIN, .rawMax = PRESS_SENSOR_RAW_MAX,
                .defaultValue = 0.0, .format = "%.1f" , .precision = 0.05,
            },
            .pga = AdcPgaX1, .refR = 465, .convert = convertOilPress,
        },
    },
    {
        {},
        {
            .base = {
                .labelId = "rotorTemp", .frameId = "rotorTempFrame", .labelMinId = "rotorTempMin", .labelMaxId = "rotorTempMax",
                .alertLow = -25, .warningLow = -25, .notifyLow = -25,
                .notifyHigh = 800, .warningHigh = 800, .alertHigh = 900,
                .rawMin = ROTOR_TEMP_SENSOR_RAW_MIN, .rawMax = ROTOR_TEMP_SENSOR_RAW_MAX,
                .defaultValue = 10.0, .format = "%.0f" , .precision = 5,
            },
            .pga = AdcPgaX2, .refR = 468, .convert = convertRotorTemp,
        },
        {
            .base = {
                .labelId = "caliperTemp", .frameId = "caliperTempFrame", .labelMinId = "caliperTempMin", .labelMaxId = "caliperTempMax",
                .alertLow = -25, .warningLow = -25, .notifyLow = -25,
                .notifyHigh = 150, .warningHigh = 150, .alertHigh = 200,
                .rawMin = CALIPER_TEMP_SENSOR_RAW_MIN, .rawMax = CALIPER_TEMP_SENSOR_RAW_MAX,
                .defaultValue = 10.0, .format = "%.0f" , .precision = 0.5,
            },
            .pga = AdcPgaAdaptive, .refR = 20000, .convert = convertCaliperTemp,
        },
        {
            .base = { // reference voltage for sensors, not shown in UI
                .alertLow = VDD_RAW_MIN * 2, .warningLow = VDD_RAW_MIN * 2, .notifyLow = VDD_RAW_MIN * 2,
                .notifyHigh = VDD_RAW_MAX * 2, .warningHigh = VDD_RAW_MAX * 2, .alertHigh = VDD_RAW_MAX * 2,
                .rawMin = VDD_RAW_MIN, .rawMax = VDD_RAW_MAX,
                .defaultValue = VDD_DEFAULT, .format = "%.0f" , .precision = 2,
            },
            .pga = AdcPgaX1, .refR = 0, .convert = convertVdd,
        },
    }
};
