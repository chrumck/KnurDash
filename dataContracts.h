#pragma once

#include <gtk/gtk.h>
#include "adapter.h"
#include "application.h"

// #define IS_DEBUG

#define ENABLE_CANBUS TRUE

#define MIN_APP_RUNNING_TIME_US 8000000
#define MAX_REQUEST_ERROR_RATE 0.50

#define ADC_COUNT 2
#define ADC_CHANNEL_COUNT 4
#define CAN_SENSORS_COUNT 1
#define FORMATTED_READING_LENGTH 10

#define CAN_FRAME_ID_LENGTH 4
#define CAN_DATA_SIZE 8
#define CAN_FRAMES_COUNT 5

#define ADC_FRAME_ID 0x000007F0
#define ADC_FRAME_FAULTY_VALUE 0xFF
#define ADC_FRAME_TEMP_OFFSET 40
#define ADC_FRAME_PRESS_FACTOR 32
#define ADC_FRAME_ROTOR_TEMP_FACTOR 0.25
#define ADC_FRAME_CALIPER_TEMP_OFFSET 50

#define SENSOR_WARNING_ERROR_COUNT 5
#define SENSOR_CRITICAL_ERROR_COUNT 100

#define COOLANT_TEMP_CAN_SENSOR_INDEX 0

typedef struct {
    volatile gboolean isIgnOn;
    volatile gboolean wasEngineStarted;
    volatile gboolean isEngineRunning;
} System;

typedef enum {
    AppShutdown = 0,
    SystemShutdown = 1,
    SystemReboot = 2,
    AppShutdownDueToErrors = 3,
    SystemShutdownByUser = 4,
} ShutDownType;

typedef struct {
    char* labelId;
    char* frameId;
    char* labelMinId;
    char* labelMaxId;

    gdouble alertLow;
    gdouble warningLow;
    gdouble notifyLow;
    gdouble notifyHigh;
    gdouble warningHigh;
    gdouble alertHigh;

    gint32 rawMin;
    gint32 rawMax;

    gdouble defaultValue;
    char* format;
    gdouble precision;
} SensorBase;

typedef enum {
    AdcPgaX1 = 0b00,
    AdcPgaX2 = 0b01,
    AdcPgaX4 = 0b10,
    AdcPgaX8 = 0b11,
    AdcPgaAdaptive = 0b100,
} AdcPga;

static const guint8 const AdcPgaMultipliers[] = {
    [AdcPgaX1] = 1,
    [AdcPgaX2] = 2,
    [AdcPgaX4] = 4,
    [AdcPgaX8] = 8,
};

static const guint16 const AdcPgaLimits[] = {
    [AdcPgaX2] = 820,
    [AdcPgaX4] = 410,
    [AdcPgaX8] = 205,
};

typedef struct {
    SensorBase base;

    AdcPga pga;
    gint32 refR;
    gdouble(*convert)(gint32 sensorV, gint32 driveV, gint32 refR);
} AdcSensor;

typedef struct {
    SensorBase base;

    gdouble(*getValue)();
} CanSensor;

typedef enum {
    StateAlertLow = -3,
    StateWarningLow = -2,
    StateNotifyLow = -1,
    StateNormal = 0,
    StateNotifyHigh = 1,
    StateWarningHigh = 2,
    StateAlertHigh = 3,
} SensorState;

typedef struct {
    gdouble value;
    gdouble min;
    gdouble max;

    SensorState state;
    guint32 errorCount;
} SensorReading;

typedef struct {
    GtkLabel* label;
    GtkFrame* frame;
    GtkLabel* labelMin;
    GtkLabel* labelMax;
} SensorWidgets;

typedef struct {
    gint i2cPiHandle;
    gint i2cAdcHandles[ADC_COUNT];

    guint32 requestCount;
    guint32 errorCount;

    SensorReading adcReadings[ADC_COUNT][ADC_CHANNEL_COUNT];
    SensorWidgets adcWidgets[ADC_COUNT][ADC_CHANNEL_COUNT];

    SensorReading canReadings[CAN_SENSORS_COUNT];
    SensorWidgets canWidgets[CAN_SENSORS_COUNT];
} SensorData;

typedef struct {
    guint32 canId;
    guint32 refreshIntervalMillis;
} CanFrame;

typedef struct {
    guint32 canId;
    gboolean isExtended;
    gboolean isRemoteRequest;
    guint8 dataLength;
    guint8 data[CAN_DATA_SIZE];
    gint64 timestamp;

    guint32 errorCount;

    GMutex lock;

    guint btNotifyingSourceId;
    gboolean btWasSent;
} CanFrameState;

typedef struct {
    int i2cPiHandle;
    int i2cCanHandle;
    gboolean isControllerPinModeSet;

    guint32 requestCount;
    guint32 errorCount;

    GMainLoop* mainLoop;

    CanFrameState frames[CAN_FRAMES_COUNT];
    CanFrameState adcFrame;
} CanBusData;

typedef struct {
    GMainLoop* mainLoop;

    GDBusConnection* dbusConn;
    Adapter* adapter;
    Application* app;
    Advertisement* adv;

    gboolean isNotifying;
} BluetoothData;

typedef struct {
    GtkBuilder* builder;

    gint64 startupTimestamp;

    volatile gboolean minMaxResetRequested;
    volatile gboolean shutdownRequested;

    System system;
    volatile gboolean isSystemWorkerRunning;

    SensorData sensors;
    volatile gboolean isSensorWorkerRunning;

    CanBusData canBus;
    volatile gboolean isCanBusWorkerRunning;
    volatile gboolean canBusRestartRequested;

    BluetoothData bluetooth;
    volatile gboolean isBluetoothWorkerRunning;
} AppData;

typedef struct {
    GtkLabel* label;
    char value[FORMATTED_READING_LENGTH];
} SetLabelArgs;

typedef struct {
    GtkFrame* frame;
    SensorState state;
} SetFrameClassArgs;
