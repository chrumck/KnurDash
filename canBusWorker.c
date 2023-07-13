#include <gtk/gtk.h>
#include <pigpiod_if2.h>

#include "dataContracts.h"

#define CAN_ADAPTER_I2C_ADDRESS 0x25
#define CAN_ADAPTER_BAUD_REGISTER 0x03
#define CAN_ADAPTER_BAUD_VALUE 16

static gpointer canBusWorkerLoop(gpointer data) {
    WorkerData* workerData = data;

    int i2cPiHandle = pigpio_start(NULL, NULL);
    if (i2cPiHandle < 0)  g_error("Could not connect to pigpiod: %d", i2cPiHandle);
    workerData->canBusData.i2cPiHandle = i2cPiHandle;

    int i2cCanHandle = i2c_open(i2cPiHandle, 1, CAN_ADAPTER_I2C_ADDRESS, 0);
    if (i2cCanHandle < 0)  g_error("Could not get CAN adapter handle %d", i2cCanHandle);
    workerData->canBusData.i2cCanHandle = i2cCanHandle;

    int baudValue = i2c_read_byte_data(i2cPiHandle, i2cCanHandle, CAN_ADAPTER_BAUD_REGISTER);
    if (baudValue < 0)  g_error("Could not get can adapter baud value: %d", baudValue);
    if (baudValue != CAN_ADAPTER_BAUD_VALUE) {
        int writeResult = i2c_write_byte_data(i2cPiHandle, i2cCanHandle, CAN_ADAPTER_BAUD_REGISTER, CAN_ADAPTER_BAUD_VALUE);
        if (writeResult < 0) g_error("Could not set can adapter baud value: %d", writeResult);
    }

    // loop code

    i2c_close(i2cPiHandle, i2cCanHandle);
    pigpio_stop(i2cPiHandle);

    return NULL;
}