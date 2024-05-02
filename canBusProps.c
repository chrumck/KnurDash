#ifndef __canBusProps_c
#define __canBusProps_c

#include <gtk/gtk.h>

#include "dataContracts.h"
#include "appData.c"

#define BAUD_REGISTER 0x03
#define BAUD_VALUE 16

#define MASK0_REGISTER   0x60
#define MASK1_REGISTER   0x65
#define FILTER0_REGISTER 0x70
#define FILTER1_REGISTER 0x80
#define FILTER2_REGISTER 0x90
#define FILTER3_REGISTER 0xA0
#define FILTER4_REGISTER 0xB0
#define FILTER5_REGISTER 0xC0

#define GET_FRAME_REGISTER 0x40

#define RPM_FRAME_INDEX 2
#define RPM_SCALING 0.25

#define COOLANT_TEMP_FRAME_INDEX 3
#define COOLANT_TEMP_OFFSET 40

#define MASK_FILTER_LENGTH  6 // the last byte in mask/filter is the checksum
guint8 maskValue[MASK_FILTER_LENGTH] = { 0x0, 0x0, 0x0, 0x07, 0xFF, 0xFA };
guint8 filter0Value[MASK_FILTER_LENGTH] = { 0x0, 0x0, 0x0, 0x0, 0x78, 0x78 };
guint8 filter1Value[MASK_FILTER_LENGTH] = { 0x0, 0x0, 0x0, 0x0, 0x86, 0x86 };
guint8 filter2Value[MASK_FILTER_LENGTH] = { 0x0, 0x0, 0x0, 0x02, 0x02, 0x04 };
guint8 filter3Value[MASK_FILTER_LENGTH] = { 0x0, 0x0, 0x0, 0x04, 0x20, 0x24 };
guint8 filter4Value[MASK_FILTER_LENGTH] = { 0x0, 0x0, 0x0, 0x4, 0xFA, 0xFE };
guint8 filter5Value[MASK_FILTER_LENGTH] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

static const CanFrame canFrames[CAN_FRAMES_COUNT] = {
    {.canId = 0x78, .refreshIntervalMillis = 25},
    {.canId = 0x86, .refreshIntervalMillis = 25},
    {.canId = 0x202, .refreshIntervalMillis = 25},
    {.canId = 0x420, .refreshIntervalMillis = 500},
    {.canId = 0x4FA, .refreshIntervalMillis = 500},
};

gboolean isFrameTooOld(guint8 frameIndex) {
    const CanFrame* frame = &canFrames[frameIndex];
    CanFrameState* state = &appData.canBus.frames[frameIndex];

    gint64 currentTimestamp = g_get_monotonic_time();
    return state->timestamp + (3 * frame->refreshIntervalMillis * 1000) < currentTimestamp ? TRUE : FALSE;
}

gdouble getEngineRpm() {
    if (isFrameTooOld(RPM_FRAME_INDEX)) return -1;

    CanFrameState* state = &appData.canBus.frames[RPM_FRAME_INDEX];

    g_mutex_lock(&state->lock);
    gdouble retVal = (gdouble)((state->data[0] << 8 | state->data[1]) * RPM_SCALING);
    g_mutex_unlock(&state->lock);

    return retVal;
}

gdouble getCoolantTemp() {
    if (isFrameTooOld(COOLANT_TEMP_FRAME_INDEX)) return -100;

    CanFrameState* state = &appData.canBus.frames[COOLANT_TEMP_FRAME_INDEX];
    g_mutex_lock(&state->lock);
    gdouble retVal = (gdouble)(state->data[0] - COOLANT_TEMP_OFFSET);
    g_mutex_unlock(&state->lock);

    return retVal;
}

static const CanSensor canSensors[CAN_SENSORS_COUNT] = {
    {
        .base = {
            .labelId = "coolantTemp", .frameId = "coolantTempFrame", .labelMinId = "coolantTempMin", .labelMaxId = "coolantTempMax",
            .alertLow = -30, .warningLow = -30, .notifyLow = 80,
            .notifyHigh = 110, .warningHigh = 110, .alertHigh = 120,
            .rawMin = -40, .rawMax = 215,
            .defaultValue = 10.0, .format = "%.0f" , .precision = 0.3,
        },
            .getValue = getCoolantTemp,
    },
};

#endif