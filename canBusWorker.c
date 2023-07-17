#include <gtk/gtk.h>
#include <pigpiod_if2.h>

#include "dataContracts.h"
#include "workerData.c"
#include "helpers.c"
#include "canBusProps.c"

#define CANBUS_WORKER_SHUTDOWN_LOOP_INTERVAL 500

#define I2C_ADDRESS 0x25
#define I2C_REQUEST_DELAY 2e3

#define MASK_FILTER_LENGTH  5
#define FRAME_ID_LENGTH  4
#define FRAME_LENGTH  16

#define NO_FRAMES_AVAILABLE_RESPONSE 0x00000000
#define RECEIVE_REJECTED_RESPONSE 0x00000001
#define RESPONSE_NOT_READY_RESPONSE 0x00000002

void setMaskOrFilter(int piHandle, int canHandle, int i2cRegister, guint8* value) {
    guint8 maskOrFilterBuffer[MASK_FILTER_LENGTH];
    int readResult = i2c_read_i2c_block_data(piHandle, canHandle, i2cRegister, maskOrFilterBuffer, MASK_FILTER_LENGTH);
    if (readResult != MASK_FILTER_LENGTH) {
        g_warning("Could not get CAN mask/filter, register:%x, error:%d", i2cRegister, readResult);
    }

    return;

    if (!isArrayEqual(value, maskOrFilterBuffer, MASK_FILTER_LENGTH)) {
        g_message(
            "Setting CAN filter/mask, register:0x%x, values: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
            i2cRegister, value[0], value[1], value[2], value[3], value[4]
        );

        int writeResult = i2c_write_i2c_block_data(piHandle, canHandle, i2cRegister, value, MASK_FILTER_LENGTH);
        if (writeResult != 0) g_warning("Could not set CAN mask/filter, register:%x, error:%d", i2cRegister, writeResult);

        g_usleep(I2C_REQUEST_DELAY);
    }
}

gboolean stopCanBusWorker() {
    if (workerData.requestShutdown == FALSE) return G_SOURCE_CONTINUE;

    GMainContext* context = g_main_loop_get_context(workerData.canBusData.mainLoop);
    while (g_main_context_pending(context)) g_main_context_iteration(context, TRUE);

    g_main_loop_quit(workerData.canBusData.mainLoop);

    return G_SOURCE_REMOVE;
}

#define getFrameIdArray(_frameId)\
    guint8 frameIdArray[FRAME_ID_LENGTH];\
    frameIdArray[0] = (_frameId >> 24) & 0xFF;\
    frameIdArray[1] = (_frameId >> 16) & 0xFF;\
    frameIdArray[2] = (_frameId >> 8) & 0xFF;\
    frameIdArray[3] = _frameId & 0xFF;\

#define handleGetFrameError(_warningMessage, _warningMessageArg1, _warningMessageArg2) {\
    errorCount++;\
    if (!frameState->receiveFailed)  g_warning(_warningMessage, _warningMessageArg1, _warningMessageArg2);\
    if (errorCount % 20 == 0)  g_warning("CAN requests:%d, errors:%d, rate:%.4f", requestCount, errorCount, (float)errorCount / requestCount);\
    frameState->receiveFailed = TRUE;\
    return G_SOURCE_CONTINUE;\
}\

guint8 getChecksum(guint8* data, int length)
{
    guint32 sum = 0;
    for (int i = 0; i < length; i++) sum += data[i];
    if (sum > 0xff) sum = (~sum) + 1;
    sum = sum & 0xff;
    return sum;
}

gboolean getFrameFromCAN(gpointer data) {
    int frameIndex = GPOINTER_TO_INT(data);

    static guint32 requestCount = 0;
    static guint32 errorCount = 0;
    requestCount++;

    const CanFrame* frame = &canFrames[frameIndex];
    CanBusData* canBusData = &workerData.canBusData;
    CanFrameState* frameState = &canBusData->frames[frameIndex];

    getFrameIdArray(frame->canId);
    int requestResult = i2c_write_i2c_block_data(
        canBusData->i2cPiHandle,
        canBusData->i2cCanHandle,
        GET_FRAME_REGISTER,
        frameIdArray,
        FRAME_ID_LENGTH);


    if (requestResult != 0) handleGetFrameError("Failed request for CAN frame id:%x, error:%d", frame->canId, requestResult);

    g_usleep(I2C_REQUEST_DELAY);

    guint8 frameData[FRAME_LENGTH] = { 0 };
    int readResult = i2c_read_device(canBusData->i2cPiHandle, canBusData->i2cCanHandle, frameData, FRAME_LENGTH);

    if (readResult != FRAME_LENGTH) handleGetFrameError("Failed receive for CAN frame id:%x, error:%d", frame->canId, requestResult);

    guint32 receivedFrameId = frameData[0] << 24 | frameData[1] << 16 | frameData[2] << 8 | frameData[3];

    if (receivedFrameId == NO_FRAMES_AVAILABLE_RESPONSE) return G_SOURCE_CONTINUE;

    if (receivedFrameId == RECEIVE_REJECTED_RESPONSE || receivedFrameId == RESPONSE_NOT_READY_RESPONSE) {
        handleGetFrameError("Got error frame id for CAN frame id:0x%x, received:0x%x", frame->canId, receivedFrameId);
    }

    if (frameData[6] < 0 || frameData[6] > CAN_DATA_SIZE) {
        handleGetFrameError("Invalid data length for CAN frame id:0x%x, length:%d", frame->canId, frameData[6]);
    }

    if (receivedFrameId != frame->canId) {
        handleGetFrameError("Invalid received id CAN frame id:0x%x, received:0x%x", frame->canId, receivedFrameId);
    }

    guint8 checksum = getChecksum(frameData, 15);
    if (checksum != frameData[15]) {
        handleGetFrameError("Invalid checksum for CAN frame id:0x%x, expected:0x%x", frame->canId, checksum);
    }

    g_mutex_lock(&frameState->lock);

    frameState->canId = receivedFrameId;
    frameState->isExtended = frameData[4];
    frameState->isRemoteRequest = frameData[5];
    frameState->dataLength = frameData[6];
    memset(frameState->data, 0, CAN_DATA_SIZE);
    memcpy(frameState->data, frameData + 7, frameData[6]);
    frameState->receiveFailed = FALSE;
    frameState->timestamp = g_get_monotonic_time();

    g_mutex_unlock(&frameState->lock);
    return G_SOURCE_CONTINUE;
}

gpointer canBusWorkerLoop() {
    g_message("CANBUS worker starting");

    int i2cPiHandle = pigpio_start(NULL, NULL);
    if (i2cPiHandle < 0)  g_error("Could not connect to pigpiod: %d", i2cPiHandle);
    workerData.canBusData.i2cPiHandle = i2cPiHandle;

    int i2cCanHandle = i2c_open(i2cPiHandle, 1, I2C_ADDRESS, 0);
    if (i2cCanHandle < 0)  g_error("Could not get CAN handle %d", i2cCanHandle);
    workerData.canBusData.i2cCanHandle = i2cCanHandle;

    int baudValue = i2c_read_byte_data(i2cPiHandle, i2cCanHandle, BAUD_REGISTER);
    if (baudValue < 0)  g_warning("Could not get CAN baud value: %d", baudValue);
    if (baudValue != BAUD_VALUE) {
        int writeResult = i2c_write_byte_data(i2cPiHandle, i2cCanHandle, BAUD_REGISTER, BAUD_VALUE);
        if (writeResult < 0) g_warning("Could not set CAN baud value: %d", writeResult);
        g_usleep(I2C_REQUEST_DELAY);
    }

    setMaskOrFilter(i2cPiHandle, i2cCanHandle, MASK0_REGISTER, maskValue);
    setMaskOrFilter(i2cPiHandle, i2cCanHandle, MASK1_REGISTER, maskValue);
    setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER0_REGISTER, filter0Value);
    setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER1_REGISTER, filter1Value);
    setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER2_REGISTER, filter2Value);
    setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER3_REGISTER, filter3Value);
    setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER4_REGISTER, filter4Value);
    setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER5_REGISTER, filter5Value);

    GMainContext* workerContext = g_main_context_new();
    g_main_context_push_thread_default(workerContext);
    GMainLoop* mainLoop = g_main_loop_new(workerContext, FALSE);
    workerData.canBusData.mainLoop = mainLoop;

    GSource* stopCanBusWorkerSource = g_timeout_source_new(CANBUS_WORKER_SHUTDOWN_LOOP_INTERVAL);
    g_source_set_callback(stopCanBusWorkerSource, stopCanBusWorker, NULL, NULL);
    g_source_attach(stopCanBusWorkerSource, workerContext);
    g_source_unref(stopCanBusWorkerSource);

    for (int i = 0; i < CAN_FRAMES_COUNT; i++)
    {
        GSource* frameRefreshSource = g_timeout_source_new(canFrames[i].refreshIntervalMillis);
        g_source_set_callback(frameRefreshSource, getFrameFromCAN, GINT_TO_POINTER(i), NULL);
        g_source_attach(frameRefreshSource, workerContext);
        g_source_unref(frameRefreshSource);
    }

    workerData.isCanBusWorkerRunning = TRUE;

    g_main_loop_run(mainLoop);

    g_main_loop_unref(mainLoop);

    i2c_close(i2cPiHandle, i2cCanHandle);
    pigpio_stop(i2cPiHandle);

    workerData.isCanBusWorkerRunning = FALSE;
    g_message("CANBUS worker shutting down");

    return NULL;
}