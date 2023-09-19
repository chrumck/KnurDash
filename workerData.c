#ifndef __workerData_c
#define __workerData_c

#include <gtk/gtk.h>

#include "dataContracts.h"

static WorkerData workerData = {
    .canBus = {.i2cPiHandle = -1, .i2cCanHandle = -1},
    .sensors = {.i2cPiHandle = -1, .i2cAdcHandles = {-1, -1}}
};

#endif