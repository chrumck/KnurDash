#ifndef dataContracts_h
#define dataContracts_h

#include <gtk/gtk.h>

typedef struct _WorkerData {
    GtkBuilder* builder;

    gboolean isShuttingDown;

    gboolean isSensorWorkerRunning;
} WorkerData;

typedef struct _ReadChannelArgs {
    int pi;
    int adc;
    guint8 channel;
    gint32 rawMin;
    gint32 rawMax;
    char* (*convert)(gint32 raw);
    GtkLabel* label;
} ReadChannelArgs;

#endif