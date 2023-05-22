#include <gtk/gtk.h>
#include <math.h>

#include "dataContracts.h"

#define TEMP_SENSOR_RAW_MIN 30
#define TEMP_SENSOR_RAW_MAX 1830

#define PRESS_SENSOR_RAW_MIN 50
#define PRESS_SENSOR_RAW_MAX 1000

#define R_REF_OIL_TEMP 1992
#define R_REF_OIL_PRESS 465

#define TEMP_A 276.4
#define TEMP_B -39.75
#define TEMP_C 0.09174

#define PRESS_A -0.2980
#define PRESS_B 0.02548
#define PRESS_C 0.0000364

static gdouble convertTemp(gint32 vSensor, gint32 rRef, gint32 vIn) {
    gdouble rSensor = vSensor * rRef / (vIn - vSensor);
    gdouble logRSensor = log(rSensor);
    return round(TEMP_A + (TEMP_B * logRSensor) + (TEMP_C * pow(logRSensor, 3)));
}

static double convertOilTemp(gint32 vSensor, gint32 vIn) {
    return convertTemp(vSensor, R_REF_OIL_TEMP, vIn);
}

static gdouble convertOilPress(gint32 vSensor, gint32 vIn) {
    gdouble rSensor = vSensor * R_REF_OIL_PRESS / (vIn - vSensor);
    return round(PRESS_A + (PRESS_B * rSensor) + (PRESS_C * pow(rSensor, 2)));
}

static const SensorProps channelProps[2][4] = {
    {
        {
            .labelId = "transTemp", .frameId = "transTempFrame", .labelMinId = "transTempMin", .labelMaxId = "transTempMax",
            .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX,
            .alertLow = -25, .warningLow = -25, .notifyLow = -25,
            .notifyHigh = 135, .warningHigh = 135, .alertHigh = 160,
            .convert = convertOilTemp, .format = "%.0f" ,
        },
        {
            .labelId = "diffTemp", .frameId = "diffTempFrame", .labelMinId = "diffTempMin", .labelMaxId = "diffTempMax",
            .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX,
            .alertLow = -25, .warningLow = -25, .notifyLow = -25,
            .notifyHigh = 135, .warningHigh = 135, .alertHigh = 160,
            .convert = convertOilTemp, .format = "%.0f" ,
        },
        {
            .labelId = "oilTemp", .frameId = "oilTempFrame", .labelMinId = "oilTempMin", .labelMaxId = "oilTempMax",
            .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX,
            .alertLow = -25, .warningLow = -25, .notifyLow = 100,
            .notifyHigh = 125, .warningHigh = 125, .alertHigh = 135,
            .convert = convertOilTemp, .format = "%.0f" ,
        },
        {
            .labelId = "oilPress", .frameId = "oilPressFrame", .labelMinId = "oilPressMin", .labelMaxId = "oilPressMax",
            .rawMin = PRESS_SENSOR_RAW_MIN, .rawMax = PRESS_SENSOR_RAW_MAX,
            .alertLow = -25, .warningLow = -25, .notifyLow = 100,
            .notifyHigh = 125, .warningHigh = 125, .alertHigh = 135,
            .convert = convertOilPress, .format = "%.1f" ,
        },
    },
    {
        {},
        {},
        {},
        {}
    }
};
