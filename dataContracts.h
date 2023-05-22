#include <gtk/gtk.h>

#ifndef dataContracts_h
#define dataContracts_h

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

    gint32 rawMin;
    gint32 rawMax;

    gdouble alertLow;
    gdouble warningLow;
    gdouble notifyLow;
    gdouble notifyHigh;
    gdouble warningHigh;
    gdouble alertHigh;

    gdouble(*convert)(gint32 raw, gint32 vIn);
    char* format;
} SensorProps;

typedef struct _ReadingWidgets {
    GtkLabel* label;
    GtkFrame* frame;
    GtkLabel* labelMin;
    GtkLabel* labelMax;
} ReadingWidgets;

typedef struct _ChannelData {
    int piHandle;
    int adcHandle[2];

    ReadingWidgets widgets[2][4];
} ChannelData;

#endif