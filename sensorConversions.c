#include <math.h>

#define TEMP_SENSOR_RAW_MIN 30
#define TEMP_SENSOR_RAW_MAX 1830

#define ADC_CHANNEL_OIL_TEMP 0x02
#define R_REF_OIL_TEMP 1992

#define V_33 3302
#define TEMP_A 276.4
#define TEMP_B -39.75
#define TEMP_C 0.09174

static char* convertToTemp(gint32 vSensor, gint32 rRef) {
    gdouble rSensor = vSensor * rRef / (V_33 - vSensor);
    gdouble logRSensor = log(rSensor);
    gint32 tempDegC = round(TEMP_A + (TEMP_B * logRSensor) + (TEMP_C * pow(logRSensor, 3)));
    char* result = calloc(sizeof(char), 10);
    sprintf(result, "%d", tempDegC);
    return result;
}

static char* convertToOilTemp(gint32 vSensor) {
    return convertToTemp(vSensor, R_REF_OIL_TEMP);
}

static ReadChannelArgs getOilTempArgs(int pi, int adc, GtkLabel* label) {
    return (ReadChannelArgs) {
        .pi = pi,
            .adc = adc,
            .channel = ADC_CHANNEL_OIL_TEMP,
            .rawMin = TEMP_SENSOR_RAW_MIN,
            .rawMax = TEMP_SENSOR_RAW_MAX,
            .label = label,
            .convert = convertToOilTemp,
    };
}

