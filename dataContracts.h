#ifndef dataContracts_h
#define dataContracts_h

#include <gtk/gtk.h>

#define FORMATTED_READING_LENGTH 10

typedef struct _WorkerData {
    GtkBuilder* builder;

    gboolean requestMinMaxReset;
    gboolean requestShutdown;

    gboolean isSensorWorkerRunning;
} WorkerData;

typedef struct _SensorProps {
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

typedef struct _SensorWidgets {
    GtkLabel* label;
    GtkFrame* frame;
    GtkLabel* labelMin;
    GtkLabel* labelMax;
} SensorWidgets;

typedef enum _SensorState {
    StateAlertLow = -3,
    StateWarningLow = -2,
    StateNotifyLow = -1,
    StateNormal = 0,
    StateNotifyHigh = 1,
    StateWarningHigh = 2,
    StateAlertHigh = 3,
} SensorState;

typedef struct _SensorData {
    int piHandle;
    int adcHandle;

    SensorState lastStates[4];
    gboolean isFaulty[4];

    gdouble lastReadings[4];
    gdouble minValues[4];
    gdouble maxValues[4];

    SensorWidgets widgets[4];
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