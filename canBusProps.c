#ifndef __canBusProps_c
#define __canBusProps_c

#include <gtk/gtk.h>

#include "dataContracts.h"
#include "workerData.c"

#define BAUD_REGISTER 0x03
#define BAUD_VALUE 16

#define MASK0_REGISTER   0x60
#define MASK1_REGISTER   0x65
#define FILTER0_REGISTER 0x70
#define FILTER1_REGISTER 0x80
#define FILTER2_REGISTER 0x90
#define FILTER3_REGISTER 0xa0
#define FILTER4_REGISTER 0xb0
#define FILTER5_REGISTER 0xc0

#define GET_FRAME_REGISTER 0x40

#define RPM_FRAME_INDEX 2
#define COOLANT_TEMP_FRAME_INDEX 3

guint8 maskValue[] = { 0x0, 0x0, 0x0, 0x07, 0xFF };
guint8 filter0Value[] = { 0x0, 0x0, 0x0, 0x02, 0x02 };
guint8 filter1Value[] = { 0x0, 0x0, 0x0, 0x0, 0x86 };
guint8 filter2Value[] = { 0x0, 0x0, 0x0, 0x04, 0x20 };
guint8 filter3Value[] = { 0x0, 0x0, 0x0, 0x0, 0x78 };
guint8 filter4Value[] = { 0x0, 0x0, 0x0, 0x0, 0x0 };
guint8 filter5Value[] = { 0x0, 0x0, 0x0, 0x0, 0x0 };

static const CanFrame canFrames[CAN_FRAMES_COUNT] = {
    {.canId = 0x78, .refreshIntervalMillis = 25},
    {.canId = 0x86, .refreshIntervalMillis = 25},
    {.canId = 0x202, .refreshIntervalMillis = 25},
    {.canId = 0x420, .refreshIntervalMillis = 500},
};

gboolean isFrameTooOld(guint8 frameIndex) {
    const CanFrame* frame = &canFrames[frameIndex];
    CanFrameState* state = &workerData.canBus.frames[frameIndex];

    gint64 currentTimestamp = g_get_monotonic_time();
    return state->timestamp + (3 * frame->refreshIntervalMillis * 1000) < currentTimestamp ? TRUE : FALSE;
}

gdouble getEngineRpm() {
    if (isFrameTooOld(RPM_FRAME_INDEX)) return -1;

    CanFrameState* state = &workerData.canBus.frames[RPM_FRAME_INDEX];

    g_mutex_lock(&state->lock);
    gdouble retVal = (gdouble)((state->data[0] << 8 | state->data[1]) / 4);
    g_mutex_unlock(&state->lock);

    return retVal;
}

gdouble getCoolantTemp() {
    if (isFrameTooOld(COOLANT_TEMP_FRAME_INDEX)) return -100;

    CanFrameState* state = &workerData.canBus.frames[COOLANT_TEMP_FRAME_INDEX];
    g_mutex_lock(&state->lock);
    gdouble retVal = (gdouble)(state->data[0] - 30);
    g_mutex_unlock(&state->lock);

    return retVal;
}

static const CanSensor canSensors[CAN_SENSORS_COUNT] = {
    {
        .base = {
            .labelId = "coolantTemp", .frameId = "coolantTempFrame", .labelMinId = "coolantTempMin", .labelMaxId = "coolantTempMax",
            .alertLow = -30, .warningLow = -30, .notifyLow = 80,
            .notifyHigh = 110, .warningHigh = 110, .alertHigh = 120,
            .rawMin = -30, .rawMax = 224,
            .format = "%.0f" , .precision = 0.3,
        },
            .getValue = getCoolantTemp,
    },
};

#endif