#ifndef __ui_c
#define __ui_c

#include <gtk/gtk.h>
#include<fcntl.h> 

#include "dataContracts.h"
#include "workerData.c"

#define BRIGHTNESS_SYSTEM_PATH "/sys/class/backlight/10-0045/brightness"
#define BRIGHTNESS_INCREMENT 32

#define CSS_CLASS_NOTIFY "notify"
#define CSS_CLASS_WARNING "warning"
#define CSS_CLASS_ALERT "alert"

GMutex guiLock;

void requestMinMaxReset()
{
    workerData.requestMinMaxReset = TRUE;
}

void setBrightness(gboolean isUp)
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

void setBrightnessDown() { setBrightness(FALSE); }

void setBrightnessUp() { setBrightness(TRUE); }

gboolean shutDown(gboolean systemShutdown)
{
    g_message("KnurDash shutdown request received");

    workerData.requestShutdown = TRUE;

    while (workerData.isSensorWorkerRunning == TRUE ||
        workerData.isCanBusWorkerRunning == TRUE ||
        workerData.isBluetoothWorkerRunning == TRUE) {
        while (gtk_events_pending()) gtk_main_iteration();
        g_usleep(100000);
    }

    while (gtk_events_pending()) gtk_main_iteration();

    gtk_main_quit();

#ifndef IS_DEBUG
    if (systemShutdown) {
        g_message("Shutting down the system");
        system("sudo shutdown now");
    }
#endif

    return FALSE;
}

gboolean systemShutDown() { return shutDown(TRUE); }
gboolean windowShutDown() { return shutDown(FALSE); }

void buttonShutDown(GtkWidget* button) {
    g_mutex_lock(&guiLock);
    gtk_widget_set_sensitive(button, FALSE);
    gtk_button_set_label(GTK_BUTTON(button), "Turning Off...");
    g_mutex_unlock(&guiLock);

    shutDown(TRUE);
}

gboolean setLabelText(gpointer data) {
    SetLabelArgs* args = data;

    g_mutex_lock(&guiLock);
    gtk_label_set_label(args->label, args->value);
    g_mutex_unlock(&guiLock);

    free(data);
    return FALSE;
}

gboolean setFrameClass(gpointer data) {
    SetFrameClassArgs* args = (SetFrameClassArgs*)data;

    g_mutex_lock(&guiLock);
    GtkStyleContext* context = gtk_widget_get_style_context(GTK_WIDGET(args->frame));

    if (gtk_style_context_has_class(context, CSS_CLASS_NOTIFY)) gtk_style_context_remove_class(context, CSS_CLASS_NOTIFY);
    if (gtk_style_context_has_class(context, CSS_CLASS_WARNING)) gtk_style_context_remove_class(context, CSS_CLASS_WARNING);
    if (gtk_style_context_has_class(context, CSS_CLASS_ALERT)) gtk_style_context_remove_class(context, CSS_CLASS_ALERT);

    if (args->state == StateNotifyLow || args->state == StateNotifyHigh) gtk_style_context_add_class(context, CSS_CLASS_NOTIFY);
    if (args->state == StateWarningLow || args->state == StateWarningHigh) gtk_style_context_add_class(context, CSS_CLASS_WARNING);
    if (args->state == StateAlertLow || args->state == StateAlertHigh) gtk_style_context_add_class(context, CSS_CLASS_ALERT);

    g_mutex_unlock(&guiLock);

    free(data);
    return FALSE;
}

#endif