#ifndef __dataContracts_h
#define __dataContracts_h

#define IS_DEBUG

#include <gtk/gtk.h>

#include "adapter.h"
#include "application.h"

#define MAX_REQUEST_ERROR_RATE 0.50

#define ADC_COUNT 2
#define ADC_CHANNEL_COUNT 4
#define CAN_SENSORS_COUNT 1
#define FORMATTED_READING_LENGTH 10

#define CAN_FRAME_ID_LENGTH 4
#define CAN_DATA_SIZE 8
#define CAN_FRAMES_COUNT 4

#define ADC_FRAME_ID 0x000007F0
#define ADC_FRAME_FAULTY_VALUE 0xFF
#define ADC_FRAME_TEMP_OFFSET 30
#define ADC_FRAME_PRESS_FACTOR 32

typedef enum {
    AppShutdown = 0,
    SystemShutdown = 1,
    SystemReboot = 2,
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

    char* format;
    gdouble precision;
} SensorBase;

typedef struct {
    SensorBase base;

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
    gboolean isFaulty;
} SensorReading;

typedef struct {
    GtkLabel* label;
    GtkFrame* frame;
    GtkLabel* labelMin;
    GtkLabel* labelMax;
} SensorWidgets;

typedef struct {
    int i2cPiHandle;
    int i2cAdcHandles[ADC_COUNT];

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
    gboolean receiveFailed;
    gint64 timestamp;

    GMutex lock;

    guint btNotifyingSourceId;
    gboolean btWasSent;
} CanFrameState;

typedef struct {
    int i2cPiHandle;
    int i2cCanHandle;

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

    gboolean isConnected;
    gboolean isNotifying;
} BluetoothData;

typedef struct {
    GtkBuilder* builder;

    gboolean requestMinMaxReset;
    gboolean requestShutdown;

    gboolean wasEngineStarted;

    SensorData sensors;
    gboolean isSensorWorkerRunning;

    CanBusData canBus;
    gboolean isCanBusWorkerRunning;

    BluetoothData bluetooth;
    gboolean isBluetoothWorkerRunning;
} WorkerData;

typedef struct {
    GtkLabel* label;
    char value[FORMATTED_READING_LENGTH];
} SetLabelArgs;

typedef struct {
    GtkFrame* frame;
    SensorState state;
} SetFrameClassArgs;

#endif