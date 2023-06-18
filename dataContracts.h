#ifndef dataContracts_h
#define dataContracts_h

#include <gtk/gtk.h>

#include "adapter.h"
#include "application.h"

#define ADC_COUNT 2
#define ADC_CHANNEL_COUNT 4
#define FORMATTED_READING_LENGTH 10

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
    int piHandle;
    int adcHandle[ADC_COUNT];

    SensorReading readings[ADC_COUNT][ADC_CHANNEL_COUNT];
    SensorWidgets widgets[ADC_COUNT][ADC_CHANNEL_COUNT];
} SensorData;

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