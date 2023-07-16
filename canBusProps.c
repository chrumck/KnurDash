#ifndef __canBusProps_c
#define __canBusProps_c

#include <gtk/gtk.h>

#include "dataContracts.h"
#include "workerData.c"

#define KNURDASH_FRAME_ID 0x7F0

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

guint8 maskValue[] = { 0x0, 0x0, 0x0, 0x07, 0xFF };
guint8 filter0Value[] = { 0x0, 0x0, 0x0, 0x02, 0x02 };
guint8 filter1Value[] = { 0x0, 0x0, 0x0, 0x0, 0x86 };
guint8 filter2Value[] = { 0x0, 0x0, 0x0, 0x04, 0x20 };
guint8 filter3Value[] = { 0x0, 0x0, 0x0, 0x0, 0x78 };
guint8 filter4Value[] = { 0x0, 0x0, 0x0, 0x0, 0x0 };
guint8 filter5Value[] = { 0x0, 0x0, 0x0, 0x0, 0x0 };

static const CanFrame canFrames[CAN_FRAMES_COUNT] = {
    {.canId = 0x78, .refreshIntervalMillis = 33},
    {.canId = 0x86, .refreshIntervalMillis = 33},
    {.canId = 0x202, .refreshIntervalMillis = 33},
    {.canId = 0x420, .refreshIntervalMillis = 500},
};

#define RPM_SIGNAL_FRAME_INDEX 2

gint32 getEngineRpm() {
    const CanFrame* frame = &canFrames[RPM_SIGNAL_FRAME_INDEX];
    CanFrameState* frameState = &workerData.canBusData.frames[RPM_SIGNAL_FRAME_INDEX];

    gint64 currentTimestamp = g_get_monotonic_time();
    if (frameState->timestamp + (3 * frame->refreshIntervalMillis * 1000) < currentTimestamp) return -1;

    return (frameState->data[0] << 8 | frameState->data[1]) / 4;
}

#endif