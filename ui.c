#ifndef ui_c
#define ui_c

#include <gtk/gtk.h>
#include<fcntl.h> 

#include "dataContracts.h"

#define BRIGHTNESS_SYSTEM_PATH "/sys/class/backlight/10-0045/brightness"
#define BRIGHTNESS_INCREMENT 32

static void setBrightness(gboolean isUp)
{
    const int fileDesc = open(BRIGHTNESS_SYSTEM_PATH, O_RDWR);
    if (fileDesc < 0)
    {
        g_warning("Could not access " BRIGHTNESS_SYSTEM_PATH);
        return;
    }

    char brightnessString[10];
    const int readResult = read(fileDesc, brightnessString, sizeof(brightnessString));
    if (readResult < 0) {
        g_warning("Could not read " BRIGHTNESS_SYSTEM_PATH);
        close(fileDesc);
        return;
    }

    int brightness;
    sscanf(brightnessString, "%d", &brightness);

    brightness += isUp == TRUE ? BRIGHTNESS_INCREMENT : -BRIGHTNESS_INCREMENT;
    brightness = brightness > 255 ? 255 : brightness < 0 ? 0 : brightness;

    sprintf(brightnessString, "%d\n", brightness);

    const int writeResult = write(fileDesc, brightnessString, strlen(brightnessString));
    if (writeResult < 0) {
        g_warning("Could not write " BRIGHTNESS_SYSTEM_PATH);
    }

    close(fileDesc);
}

static void setBrightnessDown() { setBrightness(FALSE); }

static void setBrightnessUp() { setBrightness(TRUE); }

static void shutDown(gpointer data)
{
    WorkerData* workerData = (WorkerData*)data;
    workerData->isShuttingDown = TRUE;

    while (workerData->isSensorWorkerRunning == TRUE) {
        while (gtk_events_pending()) gtk_main_iteration();
        g_usleep(100000);
    }

    while (gtk_events_pending()) gtk_main_iteration();
#ifdef NDEBUG
    system("sudo shutdown now");
#else
    gtk_main_quit();
#endif
}

static void buttonShutDown(GtkWidget* button, gpointer data) {
    gtk_widget_set_sensitive(button, FALSE);
    gtk_button_set_label(GTK_BUTTON(button), "Turning Off...");
    shutDown(data);
}

static void windowShutDown(GtkWidget* window, gpointer data) { shutDown(data); }

GMutex guiLock;
static gboolean setLabelText(gpointer data) {
    SetLabelArgs* args = (SetLabelArgs*)data;

    g_mutex_lock(&guiLock);
    gtk_label_set_label(args->label, args->value);
    g_mutex_unlock(&guiLock);

    return FALSE;
}

#endif