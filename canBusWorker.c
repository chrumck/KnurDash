#include <gtk/gtk.h>
#include <pigpiod_if2.h>

#include "dataContracts.h"
#include "helpers.c"

#define I2C_ADDRESS 0x25
#define I2C_DELAY_AFTER_WRITE 2000

#define BAUD_REGISTER 0x03
#define BAUD_VALUE 16

#define MASK_FILTER_LENGTH  5
#define MASK0_REGISTER   0x60
#define MASK1_REGISTER   0x65
#define FILTER0_REGISTER 0x70
#define FILTER1_REGISTER 0x80
#define FILTER2_REGISTER 0x90
#define FILTER3_REGISTER 0xa0
#define FILTER4_REGISTER 0xb0
#define FILTER5_REGISTER 0xc0

guint8 maskValue[] = { 0x0, 0x0, 0x0, 0x07, 0xFF };
guint8 filter0Value[] = { 0x0, 0x0, 0x0, 0x02, 0x02 };
guint8 filter1Value[] = { 0x0, 0x0, 0x0, 0x0, 0x86 };
guint8 filter2Value[] = { 0x0, 0x0, 0x0, 0x04, 0x20 };
guint8 filter3Value[] = { 0x0, 0x0, 0x0, 0x0, 0x78 };
guint8 filter4Value[] = { 0x0, 0x0, 0x0, 0x0, 0x0 };
guint8 filter5Value[] = { 0x0, 0x0, 0x0, 0x0, 0x0 };


void setMaskOrFilter(int piHandle, int canHandle, int i2cRegister, guint8* value) {
    guint8 maskOrFilterBuffer[MASK_FILTER_LENGTH];
    int readResult = i2c_read_i2c_block_data(piHandle, canHandle, i2cRegister, maskOrFilterBuffer, MASK_FILTER_LENGTH);
    if (readResult != MASK_FILTER_LENGTH) {
        g_warning("Could not get CAN mask/filter, register:%x, error:%d", i2cRegister, readResult);
    }

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

gpointer canBusWorkerLoop(gpointer data) {
    WorkerData* workerData = data;

    g_message("CANBUS worker starting");

    int i2cPiHandle = pigpio_start(NULL, NULL);
    if (i2cPiHandle < 0)  g_error("Could not connect to pigpiod: %d", i2cPiHandle);
    workerData->canBusData.i2cPiHandle = i2cPiHandle;

    int i2cCanHandle = i2c_open(i2cPiHandle, 1, I2C_ADDRESS, 0);
    if (i2cCanHandle < 0)  g_error("Could not get CAN adapter handle %d", i2cCanHandle);
    workerData->canBusData.i2cCanHandle = i2cCanHandle;

    int baudValue = i2c_read_byte_data(i2cPiHandle, i2cCanHandle, BAUD_REGISTER);
    if (baudValue < 0)  g_warning("Could not get can adapter baud value: %d", baudValue);
    if (baudValue != BAUD_VALUE) {
        int writeResult = i2c_write_byte_data(i2cPiHandle, i2cCanHandle, BAUD_REGISTER, BAUD_VALUE);
        if (writeResult < 0) g_warning("Could not set can adapter baud value: %d", writeResult);
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

    // loop goes here

    i2c_close(i2cPiHandle, i2cCanHandle);
    pigpio_stop(i2cPiHandle);

    return NULL;
}