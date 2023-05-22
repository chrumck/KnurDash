#ifndef dataContracts_h
#define dataContracts_h

#include <gtk/gtk.h>


typedef struct _WorkerData {
    GtkBuilder* builder;

    gboolean isShuttingDown;

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

typedef struct _SensorData {
    int piHandle;
    int adcHandle;

    gdouble lastReadings[4];
    gboolean isFaulty[4];

    SensorWidgets widgets[4];
} SensorData;

typedef struct _SetLabelArgs {
    GtkLabel* label;
    char* value;
} SetLabelArgs;

#endif