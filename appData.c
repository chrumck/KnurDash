#pragma once

#include <gtk/gtk.h>

#include "dataContracts.h"

static AppData appData = {
    .system = {.pigpioHandle = -1},
    .canBus = {.pigpioHandle = -1, .i2cCanHandle = -1},
    .sensors = {.pigpioHandle = -1, .i2cAdcHandles = {-1, -1}}
};
