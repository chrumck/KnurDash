#ifndef dataContracts_h
#define dataContracts_h

#include <gtk/gtk.h>

struct _WorkerData {
    GtkBuilder* builder;

    gboolean isShuttingDown;

    gboolean isSensorWorkerRunning;
};

typedef struct _WorkerData WorkerData;

#endif