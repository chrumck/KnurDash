#include <gtk/gtk.h>
#include <pigpiod_if2.h>

#include "dataContracts.h"
#include "appData.c"
#include "helpers.c"
#include "canBusProps.c"
#include "ui.c"

#define CANBUS_MAINTENANCE_LOOP_INTERVAL_MS 500
#define ENABLE_CAN_READ_ERROR_LOGGING FALSE

#define CAN_CTRL_SWITCH_GPIO_PIN 4

#define I2C_ADDRESS 0x25
#define I2C_REQUEST_DELAY_US 2000
#define I2C_SET_CONFIG_DELAY_US 100000

#define FRAME_LENGTH  16

#define NO_FRAMES_AVAILABLE_RESPONSE 0x00000000
#define RECEIVE_REJECTED_RESPONSE 0x01010101
#define RESPONSE_NOT_READY_RESPONSE 0x01010102

void setMaskOrFilter(int piHandle, int canHandle, int i2cRegister, guint8* value) {
    g_usleep(I2C_REQUEST_DELAY_US);

    int writeResult = i2c_write_byte(piHandle, canHandle, i2cRegister);
    if (writeResult != 0) {
        g_warning("Request to get CAN mask/filter failed, register:0x%x, error:%d", i2cRegister, writeResult);
        appData.canBus.errorCount++;
        return;
    }

    g_usleep(I2C_REQUEST_DELAY_US * 10);

    guint8 readBuf[MASK_FILTER_LENGTH];
    int readResult = i2c_read_device(piHandle, canHandle, readBuf, MASK_FILTER_LENGTH);
    if (readResult != MASK_FILTER_LENGTH) {
        g_warning("Could not get CAN mask/filter, register:0x%x, error:%d", i2cRegister, readResult);
        appData.canBus.errorCount++;
        return;
    }

    guint32 response = readBuf[0] << 24 | readBuf[1] << 16 | readBuf[2] << 8 | readBuf[3];
    if (response == RECEIVE_REJECTED_RESPONSE || response == RESPONSE_NOT_READY_RESPONSE) {
        g_warning("Request rejected when getting CAN mask/filter, register:0x%x, response:%X", i2cRegister, response);
        appData.canBus.errorCount++;
        return;
    }

    if (isArrayEqual(value, readBuf, MASK_FILTER_LENGTH)) return;

    g_message(
        "Setting CAN filter/mask, register:0x%x, values: 0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
        i2cRegister, value[0], value[1], value[2], value[3], value[4]
    );

    writeResult = i2c_write_i2c_block_data(piHandle, canHandle, i2cRegister, value, MASK_FILTER_LENGTH);
    if (writeResult != 0) {
        g_warning("Could not set CAN mask/filter, register:0x%x, error:%d", i2cRegister, writeResult);
        appData.canBus.errorCount++;
        return;
    }

    g_usleep(I2C_SET_CONFIG_DELAY_US);
}

gboolean startCanBus() {
    int workerStartRetriesCount = 0;
    int i2cPiHandle = appData.canBus.i2cPiHandle;
    int i2cCanHandle = appData.canBus.i2cCanHandle;

    do {
        if (workerStartRetriesCount > 0) {
            g_warning("Failed to initialize CAN controller, retrying...");
            if (i2cPiHandle >= 0) gpio_write(i2cPiHandle, CAN_CTRL_SWITCH_GPIO_PIN, TRUE);
            g_usleep(I2C_SET_CONFIG_DELAY_US * 10);
        }

        workerStartRetriesCount++;
        appData.canBus.errorCount = 0;

        if (i2cCanHandle >= 0 && i2cPiHandle >= 0) i2c_close(i2cPiHandle, i2cCanHandle);
        if (i2cPiHandle >= 0) pigpio_stop(i2cPiHandle);

        i2cPiHandle = pigpio_start(NULL, NULL);
        if (i2cPiHandle < 0) {
            g_warning("Could not connect to pigpiod: %d", i2cPiHandle);
            appData.canBus.errorCount++;
            continue;
        }

        appData.canBus.i2cPiHandle = i2cPiHandle;

        i2cCanHandle = i2c_open(i2cPiHandle, 1, I2C_ADDRESS, 0);
        if (i2cCanHandle < 0) {
            g_warning("Could not get CAN handle %d", i2cCanHandle);
            appData.canBus.errorCount++;
            continue;
        }

        appData.canBus.i2cCanHandle = i2cCanHandle;

        if (!appData.canBus.isControllerPinModeSet) {
            int setCanCtrOnPullUp = set_pull_up_down(i2cPiHandle, CAN_CTRL_SWITCH_GPIO_PIN, PI_PUD_UP);
            if (setCanCtrOnPullUp != 0) g_error("Could not set GPIO pin pulldown res for CAN_CTRL_SWITCH: %d", setCanCtrOnPullUp);

            int setCanCtrOnPinMode = set_mode(i2cPiHandle, CAN_CTRL_SWITCH_GPIO_PIN, PI_OUTPUT);
            if (setCanCtrOnPinMode != 0) g_error("Could not set GPIO pin mode for CAN_CTRL_SWITCH: %d", setCanCtrOnPinMode);

            appData.canBus.isControllerPinModeSet = TRUE;
        }

        int setCanCtrlSwitchResult = gpio_write(i2cPiHandle, CAN_CTRL_SWITCH_GPIO_PIN, FALSE);
        if (setCanCtrlSwitchResult != 0) {
            g_warning("Failed to switch CAN controller on, GPIO write result:%d", setCanCtrlSwitchResult);
            appData.canBus.errorCount++;
            continue;
        }

        g_usleep(I2C_SET_CONFIG_DELAY_US * 30);

        int baudValue = i2c_read_byte_data(i2cPiHandle, i2cCanHandle, BAUD_REGISTER);
        if (baudValue < 0) {
            g_warning("Could not get CAN baud value: %d", baudValue);
            appData.canBus.errorCount++;
            continue;
        }

        if (baudValue != BAUD_VALUE) {
            int writeResult = i2c_write_byte_data(i2cPiHandle, i2cCanHandle, BAUD_REGISTER, BAUD_VALUE);
            if (writeResult < 0) {
                g_warning("Could not set CAN baud value: %d", writeResult);
                appData.canBus.errorCount++;
                continue;
            }
        }

        g_usleep(I2C_SET_CONFIG_DELAY_US);

        setMaskOrFilter(i2cPiHandle, i2cCanHandle, MASK0_REGISTER, maskValue);
        setMaskOrFilter(i2cPiHandle, i2cCanHandle, MASK1_REGISTER, maskValue);
        setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER0_REGISTER, filter0Value);
        setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER1_REGISTER, filter1Value);
        setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER2_REGISTER, filter2Value);
        setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER3_REGISTER, filter3Value);
        setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER4_REGISTER, filter4Value);
        setMaskOrFilter(i2cPiHandle, i2cCanHandle, FILTER5_REGISTER, filter5Value);
    } while (appData.canBus.errorCount && !appData.shutdownRequested && workerStartRetriesCount < 5);

    if (appData.canBus.errorCount) {
        g_warning("Failed to initialize CAN controller after %d retries. Shutting down...", workerStartRetriesCount);
        g_idle_add(shutDown, GUINT_TO_POINTER(AppShutdownDueToErrors));
        return FALSE;
    }

    g_message("CANBUS controller initialized.");

    return TRUE;
}

gboolean restartCanBus() {
    if (appData.shutdownRequested) return G_SOURCE_REMOVE;
    if (!appData.canBusRestartRequested) return G_SOURCE_CONTINUE;

    if (appData.canBus.i2cPiHandle < 0) {
        g_warning("Cannot restart CAN, invalid i2c handle:%2d", appData.canBus.i2cPiHandle);
        appData.canBusRestartRequested = FALSE;
        return G_SOURCE_CONTINUE;
    }

    GMainContext* context = g_main_loop_get_context(appData.canBus.mainLoop);
    while (g_main_context_pending(context)) g_main_context_iteration(context, FALSE);

    g_message("Switching CAN controller off with i2c handle:%2d", appData.canBus.i2cPiHandle);

    gpio_write(appData.canBus.i2cPiHandle, CAN_CTRL_SWITCH_GPIO_PIN, TRUE);
    g_usleep(I2C_SET_CONFIG_DELAY_US * 20);

    g_message("Restarting CAN controller...");

    gboolean started = startCanBus();
    if (!started) return G_SOURCE_REMOVE;

    appData.canBusRestartRequested = FALSE;
    return G_SOURCE_CONTINUE;
}

gboolean stopCanBusWorker() {
    float errorRate = (float)appData.canBus.errorCount / appData.canBus.requestCount;
    if (errorRate > MAX_REQUEST_ERROR_RATE &&
        g_get_monotonic_time() - appData.startupTimestamp > MIN_APP_RUNNING_TIME_US &&
        !appData.shutdownRequested) {
        g_warning("CAN requests excessive error rate:%2f, shutting down app", errorRate);
        g_idle_add(shutDown, GUINT_TO_POINTER(AppShutdownDueToErrors));
    }

    if (!appData.shutdownRequested) return G_SOURCE_CONTINUE;

    GMainContext* context = g_main_loop_get_context(appData.canBus.mainLoop);
    while (g_main_context_pending(context)) g_main_context_iteration(context, FALSE);

    g_main_loop_quit(appData.canBus.mainLoop);

    return G_SOURCE_REMOVE;
}

#define getFrameIdArray(_frameId)\
    guint8 frameIdArray[CAN_FRAME_ID_LENGTH];\
    frameIdArray[0] = (_frameId >> 24) & 0xFF;\
    frameIdArray[1] = (_frameId >> 16) & 0xFF;\
    frameIdArray[2] = (_frameId >> 8) & 0xFF;\
    frameIdArray[3] = _frameId & 0xFF;\

#define handleGetFrameError(_warningMessage, _warningMessageArg1, _warningMessageArg2, _warningMessageArg3) {\
    appData.canBus.errorCount++;\
    frameState->errorCount++;\
    if (ENABLE_CAN_READ_ERROR_LOGGING && frameState->errorCount == 1) {\
         g_warning(_warningMessage, _warningMessageArg1, _warningMessageArg2, _warningMessageArg3);\
    }\
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
    if (appData.shutdownRequested) return G_SOURCE_REMOVE;
    if (appData.canBusRestartRequested) return G_SOURCE_CONTINUE;

    guint frameIndex = GPOINTER_TO_UINT(data);

    appData.canBus.requestCount++;

    const CanFrame* frame = &canFrames[frameIndex];
    CanBusData* canBus = &appData.canBus;
    CanFrameState* frameState = &canBus->frames[frameIndex];

    getFrameIdArray(frame->canId);
    int requestResult = i2c_write_i2c_block_data(
        canBus->i2cPiHandle,
        canBus->i2cCanHandle,
        GET_FRAME_REGISTER,
        frameIdArray,
        CAN_FRAME_ID_LENGTH);


    if (requestResult != 0) {
        handleGetFrameError("Failed request for CAN frame id:0x%x, error:%d", frame->canId, requestResult, NULL);
    }

    g_usleep(I2C_REQUEST_DELAY_US);

    if (appData.shutdownRequested) return G_SOURCE_REMOVE;

    guint8 frameData[FRAME_LENGTH] = { 0 };
    int readResult = i2c_read_device(canBus->i2cPiHandle, canBus->i2cCanHandle, frameData, FRAME_LENGTH);

    if (readResult != FRAME_LENGTH) {
        handleGetFrameError("Failed receive for CAN frame id:0x%x, error:%d", frame->canId, requestResult, NULL);
    }

    guint32 receivedFrameId = frameData[0] << 24 | frameData[1] << 16 | frameData[2] << 8 | frameData[3];

    if (receivedFrameId == NO_FRAMES_AVAILABLE_RESPONSE) return G_SOURCE_CONTINUE;

    if (receivedFrameId == RECEIVE_REJECTED_RESPONSE || receivedFrameId == RESPONSE_NOT_READY_RESPONSE) {
        handleGetFrameError("Got error frame id for CAN frame id:0x%x, received:0x%x", frame->canId, receivedFrameId, NULL);
    }

    if (frameData[6] < 0 || frameData[6] > CAN_DATA_SIZE) {
        handleGetFrameError("Invalid data length for CAN frame id:0x%x, length:%d", frame->canId, frameData[6], NULL);
    }

    if (receivedFrameId != frame->canId) {
        handleGetFrameError("Invalid received id CAN frame id:0x%x, received:0x%x", frame->canId, receivedFrameId, NULL);
    }

    guint8 checksum = getChecksum(frameData, 15);
    if (checksum != frameData[15]) {
        handleGetFrameError(
            "Invalid checksum for CAN frame id:0x%x, received:0x%x expected:0x%x",
            frame->canId, frameData[15], checksum);
    }

    g_mutex_lock(&frameState->lock);

    frameState->canId = receivedFrameId;
    frameState->isExtended = frameData[4];
    frameState->isRemoteRequest = frameData[5];
    frameState->dataLength = frameData[6];
    memset(frameState->data, 0, CAN_DATA_SIZE);
    memcpy(frameState->data, frameData + 7, frameData[6]);
    frameState->errorCount = 0;
    frameState->timestamp = g_get_monotonic_time();
    frameState->btWasSent = FALSE;

    g_mutex_unlock(&frameState->lock);

    return G_SOURCE_CONTINUE;
}

gpointer canBusWorkerLoop() {
    g_message("CANBUS worker starting");

    if (!ENABLE_CANBUS) {
        g_message("CANBUS disabled, worker idling");
        while (!appData.shutdownRequested) { g_usleep(I2C_SET_CONFIG_DELAY_US * 10); }
        return NULL;
    }

    gboolean started = startCanBus();
    if (!started) return NULL;

    GMainContext* workerContext = g_main_context_new();
    g_main_context_push_thread_default(workerContext);
    GMainLoop* mainLoop = g_main_loop_new(workerContext, FALSE);
    appData.canBus.mainLoop = mainLoop;

    GSource* stopCanBusWorkerSource = g_timeout_source_new(CANBUS_MAINTENANCE_LOOP_INTERVAL_MS);
    g_source_set_callback(stopCanBusWorkerSource, stopCanBusWorker, NULL, NULL);
    g_source_attach(stopCanBusWorkerSource, workerContext);
    g_source_unref(stopCanBusWorkerSource);

    GSource* restartCanBusSource = g_timeout_source_new(CANBUS_MAINTENANCE_LOOP_INTERVAL_MS);
    g_source_set_callback(restartCanBusSource, restartCanBus, NULL, NULL);
    g_source_attach(restartCanBusSource, workerContext);
    g_source_unref(restartCanBusSource);

    for (int i = 0; i < CAN_FRAMES_COUNT; i++)
    {
        GSource* frameRefreshSource = g_timeout_source_new(canFrames[i].refreshIntervalMillis);
        g_source_set_callback(frameRefreshSource, getFrameFromCAN, GUINT_TO_POINTER(i), NULL);
        g_source_attach(frameRefreshSource, workerContext);
        g_source_unref(frameRefreshSource);
    }

    appData.isCanBusWorkerRunning = TRUE;

    g_main_loop_run(mainLoop);

    g_main_loop_unref(mainLoop);

    gpio_write(appData.canBus.i2cPiHandle, CAN_CTRL_SWITCH_GPIO_PIN, TRUE);
    i2c_close(appData.canBus.i2cPiHandle, appData.canBus.i2cCanHandle);
    pigpio_stop(appData.canBus.i2cPiHandle);


    g_message(
        "CAN requests:%d, errors:%d, rate:%.4f",
        appData.canBus.requestCount,
        appData.canBus.errorCount,
        (float)appData.canBus.errorCount / appData.canBus.requestCount);

    g_message("CANBUS worker shutting down");
    appData.isCanBusWorkerRunning = FALSE;

    return NULL;
}