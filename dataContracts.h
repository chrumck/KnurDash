#ifndef dataContracts_h
#define dataContracts_h

#include <gtk/gtk.h>


typedef struct _WorkerData {
    GtkBuilder* builder;

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
} Sensor;

typedef struct _SensorWidgets {
    GtkLabel* label;
    GtkFrame* frame;
    GtkLabel* labelMin;
    GtkLabel* labelMax;
} SensorWidgets;

enum SensorState {
    AlertLow = -3,
    WarningLow = -2,
    NotifyLow = -1,
    Normal = 0,
    NotifyHigh = 1,
    WarningHigh = 2,
    AlertHigh = 3,
};

typedef struct _SensorData {
    int piHandle;
    int adcHandle;

    gint8 lastStates[4];
    gboolean isFaulty[4];

    gdouble lastReadings[4];
    gdouble minValues[4];
    gdouble maxValues[4];

    SensorWidgets widgets[4];
} SensorData;

typedef struct _SetLabelArgs {
    GtkLabel* label;
    char* value;
} SetLabelArgs;

#endif