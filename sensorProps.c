#include <gtk/gtk.h>
#include <math.h>

#include "dataContracts.h"

#define TEMP_SENSOR_RAW_MIN 30
#define TEMP_SENSOR_RAW_MAX 1830

#define PRESS_SENSOR_RAW_MIN 30
#define PRESS_SENSOR_RAW_MAX 1000

#define VDD_RAW_MIN 1550
#define VDD_RAW_MAX 1750

#define TEMP_A 276.4
#define TEMP_B -39.75
#define TEMP_C 0.09174

#define PRESS_A -0.2980
#define PRESS_B 0.02548
#define PRESS_C 0.0000364

static gdouble convertTemp(gint32 sensorV, gint32 driveV, gint32 refR) {
    const gdouble sensorR = sensorV * refR / (driveV - sensorV);
    const gdouble logSensorR = log(sensorR);
    return TEMP_A + (TEMP_B * logSensorR) + (TEMP_C * pow(logSensorR, 3));
}

static gdouble convertOilPress(gint32 sensorV, gint32 driveV, gint32 refR) {
    const gdouble sensorR = sensorV * refR / (driveV - sensorV);
    const gdouble value = PRESS_A + (PRESS_B * sensorR) + (PRESS_C * pow(sensorR, 2));
    return value < 0 ? 0 : value;
}

static gdouble convertVdd(gint32 sensorV, gint32 driveV, gint32 refR) {
    return sensorV * 2;
}

static const Sensor sensors[ADC_COUNT][ADC_CHANNEL_COUNT] = {
    {
        {
            .labelId = "transTemp", .frameId = "transTempFrame", .labelMinId = "transTempMin", .labelMaxId = "transTempMax",
            .vMin = TEMP_SENSOR_RAW_MIN, .vMax = TEMP_SENSOR_RAW_MAX,
            .alertLow = -25, .warningLow = -25, .notifyLow = -25,
            .notifyHigh = 135, .warningHigh = 135, .alertHigh = 160,
            .refR = 2012, .convert = convertTemp, .format = "%.0f" , .precision = 0.2,
        },
        {
            .labelId = "diffTemp", .frameId = "diffTempFrame", .labelMinId = "diffTempMin", .labelMaxId = "diffTempMax",
            .vMin = TEMP_SENSOR_RAW_MIN, .vMax = TEMP_SENSOR_RAW_MAX,
            .alertLow = -25, .warningLow = -25, .notifyLow = -25,
            .notifyHigh = 135, .warningHigh = 135, .alertHigh = 160,
            .refR = 2006, .convert = convertTemp, .format = "%.0f" , .precision = 0.2,
        },
        {
            .labelId = "oilTemp", .frameId = "oilTempFrame", .labelMinId = "oilTempMin", .labelMaxId = "oilTempMax",
            .vMin = TEMP_SENSOR_RAW_MIN, .vMax = TEMP_SENSOR_RAW_MAX,
            .alertLow = -25, .warningLow = -25, .notifyLow = 100,
            .notifyHigh = 125, .warningHigh = 125, .alertHigh = 135,
            .refR = 1992, .convert = convertTemp, .format = "%.0f" , .precision = 0.2,
        },
        {
            .labelId = "oilPress", .frameId = "oilPressFrame", .labelMinId = "oilPressMin", .labelMaxId = "oilPressMax",
            .vMin = PRESS_SENSOR_RAW_MIN, .vMax = PRESS_SENSOR_RAW_MAX,
            .alertLow = 0.8, .warningLow = 1.3, .notifyLow = 1.3,
            .notifyHigh = 5.0, .warningHigh = 5.0, .alertHigh = 5.5,
            .refR = 465, .convert = convertOilPress, .format = "%.1f" , .precision = 0.02,
        },
    },
    {
        {},
        {},
        {},
        {
            .vMin = VDD_RAW_MIN, .vMax = VDD_RAW_MAX,
            .alertLow = VDD_RAW_MIN * 2, .warningLow = VDD_RAW_MIN * 2, .notifyLow = VDD_RAW_MIN * 2,
            .notifyHigh = VDD_RAW_MAX * 2, .warningHigh = VDD_RAW_MAX * 2, .alertHigh = VDD_RAW_MAX * 2,
            .refR = 0, .convert = convertVdd, .format = "%.0f" , .precision = 2,
        },
    }
};
