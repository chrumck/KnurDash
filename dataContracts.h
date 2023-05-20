#include <gtk/gtk.h>

struct _WorkerData {
    GtkBuilder* builder;

    gboolean isShuttingDown;

    gboolean isSensorWorkerRunning;
};

typedef struct _WorkerData WorkerData;