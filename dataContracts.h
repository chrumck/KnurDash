#ifndef dataContracts_h
#define dataContracts_h

#include <gtk/gtk.h>

#define ADC_COUNT 2
#define ADC_CHANNEL_COUNT 4
#define FORMATTED_READING_LENGTH 10

typedef struct _WorkerData {
    GtkBuilder* builder;

    gboolean requestMinMaxReset;
    gboolean requestShutdown;

    gboolean wasEngineStarted;
    gboolean isSensorWorkerRunning;
} WorkerData;

typedef struct _Sensor {
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

typedef enum _SensorState {
    StateAlertLow = -3,
    StateWarningLow = -2,
    StateNotifyLow = -1,
    StateNormal = 0,
    StateNotifyHigh = 1,
    StateWarningHigh = 2,
    StateAlertHigh = 3,
} SensorState;

typedef struct _SensorReading {
    gdouble value;
    gdouble min;
    gdouble max;

    SensorState state;
    gboolean isFaulty;
} SensorReading;

typedef struct _SensorWidgets {
    GtkLabel* label;
    GtkFrame* frame;
    GtkLabel* labelMin;
    GtkLabel* labelMax;
} SensorWidgets;

typedef struct _SensorData {
    int piHandle;
    int adcHandle[ADC_COUNT];

    SensorReading readings[ADC_COUNT][ADC_CHANNEL_COUNT];
    SensorWidgets widgets[ADC_COUNT][ADC_CHANNEL_COUNT];
} SensorData;

typedef struct _SetLabelArgs {
    GtkLabel* label;
    char value[FORMATTED_READING_LENGTH];
} SetLabelArgs;

typedef struct _SetFrameClassArgs {
    GtkFrame* frame;
    SensorState state;
} SetFrameClassArgs;

#endif