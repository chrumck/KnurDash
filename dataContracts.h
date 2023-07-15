#ifndef __dataContracts_h
#define __dataContracts_h

#include <gtk/gtk.h>

#include "adapter.h"
#include "application.h"

#define ADC_COUNT 2
#define ADC_CHANNEL_COUNT 4
#define FORMATTED_READING_LENGTH 10

#define CAN_DATA_SIZE 8

typedef struct {
    char* labelId;
    char* frameId;
    char* labelMinId;
    char* labelMaxId;

    gint32 vMin;
    gint32 vMax;

    gdouble alertLow;
    gdouble warningLow;
    gdouble notifyLow;
    gdouble notifyHigh;
    gdouble warningHigh;
    gdouble alertHigh;

    gint32 refR;
    gdouble(*convert)(gint32 sensorV, gint32 driveV, gint32 refR);
    char* format;
    gdouble precision;
} Sensor;

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

    SensorReading readings[ADC_COUNT][ADC_CHANNEL_COUNT];
    SensorWidgets widgets[ADC_COUNT][ADC_CHANNEL_COUNT];
} SensorData;

typedef struct {
    guint32 canId;
    guint32 refreshIntervalUs;
} CanFrame;

typedef struct {
    guint32 canId;
    gboolean isExtended;
    gboolean isRemoteRequest;
    guint8 dataLength;
    guint8 data[CAN_DATA_SIZE];
    gint64 timestamp;
    gboolean isSent;
} CanFrameData;

typedef struct {
    int i2cPiHandle;
    int i2cCanHandle;
} CanBusData;

typedef struct {
    GMainLoop* mainLoop;

    GDBusConnection* connection;
    Adapter* adapter;
    Application* application;
} BluetoothData;

typedef struct {
    GtkBuilder* builder;

    gboolean requestMinMaxReset;
    gboolean requestShutdown;

    gboolean wasEngineStarted;

    SensorData sensorData;
    gboolean isSensorWorkerRunning;

    CanBusData canBusData;
    gboolean isCanBusWorkerRunning;

    BluetoothData bluetoothData;
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