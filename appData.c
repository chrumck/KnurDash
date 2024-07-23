#pragma once

#include <gtk/gtk.h>

#include "dataContracts.h"

static AppData appData = {
    .canBus = {.i2cPiHandle = -1, .i2cCanHandle = -1},
    .sensors = {.i2cPiHandle = -1, .i2cAdcHandles = {-1, -1}}
};
