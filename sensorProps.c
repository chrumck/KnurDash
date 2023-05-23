#include <gtk/gtk.h>
#include <math.h>

#include "dataContracts.h"

#define TEMP_SENSOR_RAW_MIN 30
#define TEMP_SENSOR_RAW_MAX 1830

#define PRESS_SENSOR_RAW_MIN 30
#define PRESS_SENSOR_RAW_MAX 1000

#define TEMP_A 276.4
#define TEMP_B -39.75
#define TEMP_C 0.09174

#define PRESS_A -0.2980
#define PRESS_B 0.02548
#define PRESS_C 0.0000364

static gdouble convertTemp(gint32 sensorV, gint32 driveV, gint32 refR) {
    gdouble sensorR = sensorV * refR / (driveV - sensorV);
    gdouble logSensorR = log(sensorR);
    return round(TEMP_A + (TEMP_B * logSensorR) + (TEMP_C * pow(logSensorR, 3)));
}

static gdouble convertOilPress(gint32 sensorV, gint32 driveV, gint32 refR) {
    gdouble sensorR = sensorV * refR / (driveV - sensorV);
    gdouble value = PRESS_A + (PRESS_B * sensorR) + (PRESS_C * pow(sensorR, 2));
    value = round(value * 100) / 100;
    return value < 0 ? 0 : value;
}

static const Sensor sensors[4] = {
    {
        .labelId = "transTemp", .frameId = "transTempFrame", .labelMinId = "transTempMin", .labelMaxId = "transTempMax",
        .vMin = TEMP_SENSOR_RAW_MIN, .vMax = TEMP_SENSOR_RAW_MAX,
        .alertLow = -25, .warningLow = -25, .notifyLow = -25,
        .notifyHigh = 135, .warningHigh = 135, .alertHigh = 160,
        .refR = 2012, .convert = convertTemp, .format = "%.0f" ,
    },
    {
        .labelId = "diffTemp", .frameId = "diffTempFrame", .labelMinId = "diffTempMin", .labelMaxId = "diffTempMax",
        .vMin = TEMP_SENSOR_RAW_MIN, .vMax = TEMP_SENSOR_RAW_MAX,
        .alertLow = -25, .warningLow = -25, .notifyLow = -25,
        .notifyHigh = 135, .warningHigh = 135, .alertHigh = 160,
        .refR = 2006, .convert = convertTemp, .format = "%.0f" ,
    },
    {
        .labelId = "oilTemp", .frameId = "oilTempFrame", .labelMinId = "oilTempMin", .labelMaxId = "oilTempMax",
        .vMin = TEMP_SENSOR_RAW_MIN, .vMax = TEMP_SENSOR_RAW_MAX,
        .alertLow = -25, .warningLow = -25, .notifyLow = 100,
        .notifyHigh = 125, .warningHigh = 125, .alertHigh = 135,
        .refR = 1992, .convert = convertTemp, .format = "%.0f" ,
    },
    {
        .labelId = "oilPress", .frameId = "oilPressFrame", .labelMinId = "oilPressMin", .labelMaxId = "oilPressMax",
        .vMin = PRESS_SENSOR_RAW_MIN, .vMax = PRESS_SENSOR_RAW_MAX,
        .alertLow = -25, .warningLow = -25, .notifyLow = 100,
        .notifyHigh = 125, .warningHigh = 125, .alertHigh = 135,
        .refR = 465, .convert = convertOilPress, .format = "%.1f" ,
    },
};
