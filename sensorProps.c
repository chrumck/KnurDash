#ifndef __sensorProps_c
#define __sensorProps_c

#include <gtk/gtk.h>
#include <math.h>

#include "dataContracts.h"

#define TEMP_SENSOR_RAW_MIN 30
#define TEMP_SENSOR_RAW_MAX 2030

#define PRESS_SENSOR_RAW_MIN 30
#define PRESS_SENSOR_RAW_MAX 1000

#define VDD_RAW_MIN 1550
#define VDD_RAW_MAX 1750

#define TEMP_A 276.3
#define TEMP_B -39.95
#define TEMP_C 0.09136

#define PRESS_A -0.2967
#define PRESS_B 0.02529
#define PRESS_C 0.0000350

gdouble convertTemp(gint32 sensorV, gint32 driveV, gint32 refR) {
    const gdouble sensorR = sensorV * refR / (driveV - sensorV);
    const gdouble logSensorR = log(sensorR);
    return TEMP_A + (TEMP_B * logSensorR) + (TEMP_C * pow(logSensorR, 3));
}

gdouble convertOilPress(gint32 sensorV, gint32 driveV, gint32 refR) {
    const gdouble sensorR = sensorV * refR / (driveV - sensorV);
    const gdouble value = PRESS_A + (PRESS_B * sensorR) + (PRESS_C * pow(sensorR, 2));
    return value < 0 ? 0 : value;
}

gdouble convertVdd(gint32 sensorV, gint32 driveV, gint32 refR) {
    return sensorV * 2;
}

const AdcSensor adcSensors[ADC_COUNT][ADC_CHANNEL_COUNT] = {
    {
        {
            .base = {
                .labelId = "transTemp", .frameId = "transTempFrame", .labelMinId = "transTempMin", .labelMaxId = "transTempMax",
                .alertLow = -25, .warningLow = -25, .notifyLow = -25,
                .notifyHigh = 135, .warningHigh = 135, .alertHigh = 160,
                .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX,
                .format = "%.0f" , .precision = 0.3,
            },
             .refR = 2012, .convert = convertTemp,
        },
        {
            .base = {
                .labelId = "diffTemp", .frameId = "diffTempFrame", .labelMinId = "diffTempMin", .labelMaxId = "diffTempMax",
                .alertLow = -25, .warningLow = -25, .notifyLow = -25,
                .notifyHigh = 135, .warningHigh = 135, .alertHigh = 160,
                .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX,
                .format = "%.0f" , .precision = 0.3,
            },
            .refR = 2006, .convert = convertTemp,
        },
        {
            .base = {
                .labelId = "oilTemp", .frameId = "oilTempFrame", .labelMinId = "oilTempMin", .labelMaxId = "oilTempMax",
                .alertLow = -30, .warningLow = -30, .notifyLow = 80,
                .notifyHigh = 125, .warningHigh = 125, .alertHigh = 135,
                .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX,
                .format = "%.0f" , .precision = 0.3,
            },
            .refR = 1992, .convert = convertTemp,
        },
        {
            .base = {
                .labelId = "oilPress", .frameId = "oilPressFrame", .labelMinId = "oilPressMin", .labelMaxId = "oilPressMax",
                .alertLow = 0.7, .warningLow = 1.0, .notifyLow = 1.0,
                .notifyHigh = 5.0, .warningHigh = 5.0, .alertHigh = 5.5,
                .rawMin = PRESS_SENSOR_RAW_MIN, .rawMax = PRESS_SENSOR_RAW_MAX,
                .format = "%.1f" , .precision = 0.03,
            },
            .refR = 465, .convert = convertOilPress,
        },
    },
    {
        {},
        {},
        {},
        {
            .base = {
                .alertLow = VDD_RAW_MIN * 2, .warningLow = VDD_RAW_MIN * 2, .notifyLow = VDD_RAW_MIN * 2,
                .notifyHigh = VDD_RAW_MAX * 2, .warningHigh = VDD_RAW_MAX * 2, .alertHigh = VDD_RAW_MAX * 2,
                .rawMin = VDD_RAW_MIN, .rawMax = VDD_RAW_MAX,
                .format = "%.0f" , .precision = 2,
            },
            .refR = 0, .convert = convertVdd,
        },
    }
};

#endif
