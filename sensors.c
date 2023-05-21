#include <gtk/gtk.h>
#include <pigpiod_if2.h>
#include "dataContracts.h"
#include "helpers.c"
#include "sensorConversions.c"

#define SENSOR_WORKER_LOOP_INTERVAL 100000

#define FAULTY_READING_LABEL "--"
#define ADC_DEFAULT_CONFIG 0x10
#define ADC_SWITCH_CHANNEL_SLEEP 5000
#define ADC_CHANNEL_MASK 0x60
#define ADC_READING_DEADBAND 10

#define ADC_GET_CHANNEL_VALUE(config) (((config) & ADC_CHANNEL_MASK) >> 5)
#define ADC_GET_CHANNEL_BITS(channel) (((channel) << 5) & ADC_CHANNEL_MASK)

#define HANDLE_FAULT() if (isFaulty[args->channel] == TRUE) return; \
                              gtk_label_set_label(args->label, FAULTY_READING_LABEL); \
                              isFaulty[args->channel] = TRUE;

static void readChannel(const ReadChannelArgs* args) {
    static gboolean isFaulty[3];

    guint8 newConfig = ADC_DEFAULT_CONFIG | ADC_GET_CHANNEL_BITS(args->channel);
    guint8 writeResult = i2c_write_byte(args->pi, args->adc, newConfig);
    if (writeResult < 0) {
        HANDLE_FAULT();
        g_warning("Could not write config to adc: %d", writeResult);
        return;
    }

    g_usleep(ADC_SWITCH_CHANNEL_SLEEP);

    guint8 buf[3];
    guint8 readResult = i2c_read_device(args->pi, args->adc, buf, 3);
    if (readResult != 3) {
        HANDLE_FAULT();
        g_warning("Could not read adc bytes: %d", readResult);
        return;
    }

    if (ADC_GET_CHANNEL_VALUE(buf[2]) != args->channel) {
        HANDLE_FAULT();
        g_warning("Channel received does not match required: %d", args->channel);
        return;
    }

    guint32 temp = buf[0] << 8 | buf[1];
    gint32 raw = sign_extend32(temp, 12);

    if ((isFaulty[args->channel] == FALSE && (raw < args->rawMin || raw > args->rawMax)) ||
        (isFaulty[args->channel] == TRUE && (raw < args->rawMin + ADC_READING_DEADBAND
            || raw > args->rawMax - ADC_READING_DEADBAND))) {
        HANDLE_FAULT();
        g_warning("Raw value out of bounds: %d", raw);
        return;
    }

    gtk_label_set_label(args->label, args->convert(raw));
    isFaulty[args->channel] = FALSE;
}

static gpointer sensorWorkerLoop(gpointer data) {
    WorkerData* workerData = (WorkerData*)data;


    const GObject* oilTempLabel = gtk_builder_get_object(workerData->builder, "oilTemp");
    const GObject* oilTempMinLabel = gtk_builder_get_object(workerData->builder, "oilTempMin");
    const GObject* oilTempMaxLabel = gtk_builder_get_object(workerData->builder, "oilTempMax");
    const GObject* oilPressLabel = gtk_builder_get_object(workerData->builder, "oilPress");
    const GObject* oilPressMinLabel = gtk_builder_get_object(workerData->builder, "oilPressMin");
    const GObject* oilPressMaxLabel = gtk_builder_get_object(workerData->builder, "oilPressMax");

    int pi = pigpio_start(NULL, NULL);
    if (pi < 0)  g_error("Could not connect to pigpiod: %d", pi);

    int adc = i2c_open(pi, 1, 0x6e, 0);
    if (adc < 0)  g_error("Could not get adc handle %d", adc);

    g_message("Sensor worker starting");

    workerData->isSensorWorkerRunning = TRUE;
    while (workerData->isShuttingDown == FALSE) {
        g_usleep(SENSOR_WORKER_LOOP_INTERVAL);

        readChannel(&((ReadChannelArgs) { .pi = pi, .adc = adc, .channel = ADC_CHANNEL_OIL_TEMP, .rawMin = TEMP_SENSOR_RAW_MIN, .rawMax = TEMP_SENSOR_RAW_MAX, }));
    }

    g_message("Sensor worker shutting down");

    i2c_close(pi, adc);
    pigpio_stop(pi);

    workerData->isSensorWorkerRunning = FALSE;

    return NULL;
}