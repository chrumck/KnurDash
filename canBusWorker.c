#include <gtk/gtk.h>
#include <pigpiod_if2.h>

#include "dataContracts.h"
#include "helpers.c"
#include "canBusProps.c"

#define CANBUS_WORKER_SHUTDOWN_LOOP_INTERVAL 500

#define I2C_ADDRESS 0x25
#define I2C_DELAY_AFTER_WRITE 2e3

#define MASK_FILTER_LENGTH  5

guint hashCanFrame(gconstpointer value) {

}

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
        g_usleep(I2C_DELAY_AFTER_WRITE);
    }
}

gboolean stopCanBusWorker() {
    if (workerData.requestShutdown == FALSE) return G_SOURCE_CONTINUE;

    GMainContext* context = g_main_loop_get_context(workerData.canBusData.mainLoop);
    while (g_main_context_pending(context)) g_main_context_iteration(context, TRUE);

    g_main_loop_quit(workerData.canBusData.mainLoop);

    return G_SOURCE_REMOVE;
}

gboolean refreshFrame(guint frameIndex) {


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
        g_usleep(I2C_DELAY_AFTER_WRITE);
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

    workerData.isCanBusWorkerRunning = TRUE;

    g_main_loop_run(mainLoop);

    g_main_loop_unref(mainLoop);

    i2c_close(i2cPiHandle, i2cCanHandle);
    pigpio_stop(i2cPiHandle);

    workerData.isCanBusWorkerRunning = FALSE;
    g_message("CANBUS worker shutting down");

    return NULL;
}