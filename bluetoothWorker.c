#include <gtk/gtk.h>
#include <stdio.h>
#include <signal.h>

#include "adapter.h"
#include "device.h"
#include "agent.h"
#include "application.h"
#include "advertisement.h"
#include "utility.h"
#include "parser.h"
#include "logger.h"

#include "dataContracts.h"
#include "helpers.c"
#include "appData.c"
#include "sensorProps.c"

#define SERVICE_BT_NAME "KnurDash BLE" 
#define SERVICE_ID "00001ff8-0000-1000-8000-00805f9b34fb" 
#define CHAR_ID_MAIN "00000001-0000-1000-8000-00805f9b34fb"
#define CHAR_ID_FILTER "00000002-0000-1000-8000-00805f9b34fb"

#define BLUETOOTH_WORKER_SHUTDOWN_LOOP_INTERVAL 500
#define BLUETOOTH_NOTIFY_INTERVAL_MIN 5
#define BLUETOOTH_NOTIFY_INTERVAL_MAX 5000

#define RACECHRONO_DENY_ALL 0x00
#define RACECHRONO_ALLOW_ALL 0x01
#define RACECHRONO_ALLOW_SINGLE 0x02

void onPoweredStateChanged(Adapter* adapter, gboolean state) {
    g_message("BT adapter powered '%s' (%s)", state ? "on" : "off", binc_adapter_get_path(adapter));
    if (state == TRUE) return;

    g_message("Powering up BT adapter (%s)", binc_adapter_get_path(adapter));
    binc_adapter_power_on(adapter);
}

void onCentralStateChanged(Adapter* adapter, Device* device) {
    g_message("Remote central %s is %s", binc_device_get_address(device), binc_device_get_connection_state_name(device));

    ConnectionState state = binc_device_get_connection_state(device);
    if (state == BINC_CONNECTED) binc_adapter_stop_advertising(adapter, appData.bluetooth.adv);
    else if (state == BINC_DISCONNECTED) binc_adapter_start_advertising(adapter, appData.bluetooth.adv);

}

GByteArray* getArrayToSend(CanFrameState* frame) {
    GByteArray* arrayToSend = g_byte_array_sized_new(CAN_FRAME_ID_LENGTH + CAN_DATA_SIZE);

    g_mutex_lock(&frame->lock);

    guint8 frameId[CAN_FRAME_ID_LENGTH] = {
        frame->canId & 0xFF,
        (frame->canId >> 8) & 0xFF,
        (frame->canId >> 16) & 0xFF,
        (frame->canId >> 24) & 0xFF,
    };

    g_byte_array_append(arrayToSend, frameId, CAN_FRAME_ID_LENGTH);
    g_byte_array_append(arrayToSend, frame->data, CAN_DATA_SIZE);

    frame->btWasSent = TRUE;

    g_mutex_unlock(&frame->lock);

    return arrayToSend;
}

const char* onCharRead(const Application* app, const char* address, const char* serviceId, const char* charId) {
    if (!g_str_equal(serviceId, SERVICE_ID) || !g_str_equal(charId, CHAR_ID_MAIN)) return BLUEZ_ERROR_NOT_PERMITTED;

    CanFrameState* frameToSend = !appData.canBus.adcFrame.btWasSent ? &appData.canBus.adcFrame : NULL;
    for (guint i = 0; i < CAN_FRAMES_COUNT; i++)
    {
        CanFrameState* frame = &appData.canBus.frames[i];
        if (frame->btWasSent) continue;
        if (frameToSend != NULL && frame->timestamp <= frameToSend->timestamp) continue;
        frameToSend = frame;
    }

    if (frameToSend == NULL) {
#ifdef IS_DEBUG
        g_message("Received BT read request, but no frame ready to be sent");
#endif
        return NULL;
    }

    binc_application_set_char_value(appData.bluetooth.app, serviceId, charId, getArrayToSend(frameToSend));
#ifdef IS_DEBUG
    g_message("Received BT read request, sending frame:0x%x", frameToSend->canId);
#endif

    return NULL;
}

gboolean sendCanFrameToBt(gpointer data) {
    CanFrameState* frame = (CanFrameState*)data;

    if (appData.shutdownRequested) {
        frame->btNotifyingSourceId = 0;
        return G_SOURCE_REMOVE;
    }

    if (!appData.bluetooth.isNotifying || frame->btWasSent) return G_SOURCE_CONTINUE;

    GByteArray* arrayToSend = getArrayToSend(frame);
    binc_application_notify(appData.bluetooth.app, SERVICE_ID, CHAR_ID_MAIN, arrayToSend);
    g_byte_array_free(arrayToSend, TRUE);

    return G_SOURCE_CONTINUE;
}

void addSource(GMainContext* context, CanFrameState* frame, guint notifyInterval) {
    GSource* source = g_timeout_source_new(notifyInterval);
    g_source_set_callback(source, sendCanFrameToBt, frame, NULL);
    frame->btNotifyingSourceId = g_source_attach(source, context);
    g_source_unref(source);
}

void removeSource(GMainContext* context, CanFrameState* frame) {
    if (frame->btNotifyingSourceId <= 0) return;
    GSource* sourceToRemove = g_main_context_find_source_by_id(context, frame->btNotifyingSourceId);
    if (sourceToRemove != NULL) g_source_destroy(sourceToRemove);
    frame->btNotifyingSourceId = 0;
}

#define getNotifyInterval(_receivedData)\
    guint16 notifyInterval = _receivedData[1] << 8 | _receivedData[2];\
    if (notifyInterval < BLUETOOTH_NOTIFY_INTERVAL_MIN || notifyInterval > BLUETOOTH_NOTIFY_INTERVAL_MAX) {\
        g_warning("Received invalid BT notify interval:%d", notifyInterval);\
        return BLUEZ_ERROR_REJECTED;\
    }\

const char* onCharWrite(const Application* app, const char* address, const char* serviceId, const char* charId, GByteArray* received) {
    if (!g_str_equal(serviceId, SERVICE_ID) || !g_str_equal(charId, CHAR_ID_FILTER)) return BLUEZ_ERROR_NOT_PERMITTED;
    if (received->len != 1 && received->len != 3 && received->len != 7) return BLUEZ_ERROR_REJECTED;
    if (received->len == 1 && received->data[0] != RACECHRONO_DENY_ALL) return BLUEZ_ERROR_REJECTED;
    if (received->len == 3 && received->data[0] != RACECHRONO_ALLOW_ALL) return BLUEZ_ERROR_REJECTED;
    if (received->len == 7 && received->data[0] != RACECHRONO_ALLOW_SINGLE) return BLUEZ_ERROR_REJECTED;

    g_message("Received BT write request, command:%d, data length:%d", received->data[0], received->len);

    GMainContext* context = g_main_loop_get_context(appData.bluetooth.mainLoop);

    if (received->len == 1) {
        g_message("BT request to deny all");

        removeSource(context, &appData.canBus.adcFrame);
        for (guint i = 0; i < CAN_FRAMES_COUNT; i++) removeSource(context, &appData.canBus.frames[i]);

        return NULL;
    }

    getNotifyInterval(received->data);

    if (received->len == 3) {
        g_message("BT request to allow all, interval: %dms", notifyInterval);

        removeSource(context, &appData.canBus.adcFrame);
        for (guint i = 0; i < CAN_FRAMES_COUNT; i++) removeSource(context, &appData.canBus.frames[i]);

        addSource(context, &appData.canBus.adcFrame, notifyInterval);
        for (guint i = 0; i < CAN_FRAMES_COUNT; i++) addSource(context, &appData.canBus.frames[i], notifyInterval);

        return NULL;
    }

    guint32 frameId = received->data[3] << 24 | received->data[4] << 16 | received->data[5] << 8 | received->data[6];
    CanFrameState* frame = frameId == ADC_FRAME_ID ? &appData.canBus.adcFrame : NULL;
    for (guint i = 0; i < CAN_FRAMES_COUNT; i++) if (frameId == appData.canBus.frames[i].canId) frame = &appData.canBus.frames[i];

    if (frame == NULL) {
        g_warning("No frame found for BT notify request:0x%x, rejecting request", frameId);
        return BLUEZ_ERROR_REJECTED;
    }

    g_message("BT request to allow single, interval:%d, frame:0x%x", notifyInterval, frame->canId);

    removeSource(context, frame);
    addSource(context, frame, notifyInterval);

    return NULL;
}

void onCharStartNotify(const Application* app, const char* serviceId, const char* charId) {
    if (!g_str_equal(serviceId, SERVICE_ID) || !g_str_equal(charId, CHAR_ID_MAIN)) return;

    appData.bluetooth.isNotifying = TRUE;
    g_message("BT notify start");
}

void onCharStopNotify(const Application* app, const char* serviceId, const char* charId) {
    if (!g_str_equal(serviceId, SERVICE_ID) || !g_str_equal(charId, CHAR_ID_MAIN)) return;

    appData.bluetooth.isNotifying = FALSE;
    g_message("BT notify stop");
}

gboolean stopBtWorker() {
    if (!appData.shutdownRequested) return G_SOURCE_CONTINUE;

    GMainContext* context = g_main_loop_get_context(appData.bluetooth.mainLoop);
    while (g_main_context_pending(context)) g_main_context_iteration(context, TRUE);

    Adapter* adapter = appData.bluetooth.adapter;

    if (adapter != NULL) {
        GList* connected = binc_adapter_get_connected_devices(adapter);
        for (GList* iterator = connected; iterator; iterator = iterator->next) {
            Device* device = (Device*)iterator->data;
            binc_device_disconnect(device);
            g_message("Disconnecting central %s", binc_device_get_address(device));
        }
        g_list_free(connected);
    }

    if (appData.bluetooth.app != NULL) {
        binc_adapter_unregister_application(adapter, appData.bluetooth.app);
        binc_application_free(appData.bluetooth.app);
        appData.bluetooth.app = NULL;
    }

    if (appData.bluetooth.adv != NULL) {
        binc_adapter_stop_advertising(adapter, appData.bluetooth.adv);
        binc_advertisement_free(appData.bluetooth.adv);
    }

    if (adapter != NULL) {
        binc_adapter_free(adapter);
        appData.bluetooth.adapter = NULL;
    }

    if (appData.bluetooth.dbusConn != NULL) {
        g_dbus_connection_close_sync(appData.bluetooth.dbusConn, NULL, NULL);
        g_object_unref(appData.bluetooth.dbusConn);
    }

    g_main_loop_quit(appData.bluetooth.mainLoop);

    g_message("Bluetooth Worker stopBtWorker done");

    return G_SOURCE_REMOVE;
}

gpointer bluetoothWorkerLoop() {
    log_set_level(LOG_INFO);

    GDBusConnection* dbusConn = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    appData.bluetooth.dbusConn = dbusConn;

    Adapter* adapter = binc_adapter_get_default(dbusConn);
    appData.bluetooth.adapter = adapter;

    if (adapter == NULL) {
        logError("BluetoothWorker:no adapter found, shutting down...");
        return;
    }

    g_message("Bluetooth worker starting, adapter '%s'", binc_adapter_get_path(adapter));

    GMainContext* workerContext = g_main_context_new();
    g_main_context_push_thread_default(workerContext);
    GMainLoop* mainLoop = g_main_loop_new(workerContext, FALSE);
    appData.bluetooth.mainLoop = mainLoop;

    GSource* stopBtWorkerSource = g_timeout_source_new(BLUETOOTH_WORKER_SHUTDOWN_LOOP_INTERVAL);
    g_source_set_callback(stopBtWorkerSource, stopBtWorker, NULL, NULL);
    g_source_attach(stopBtWorkerSource, workerContext);
    g_source_unref(stopBtWorkerSource);

    binc_adapter_set_powered_state_cb(adapter, &onPoweredStateChanged);
    if (!binc_adapter_get_powered_state(adapter)) binc_adapter_power_on(adapter);

    binc_adapter_set_remote_central_cb(adapter, &onCentralStateChanged);

    GPtrArray* advServiceUuids = g_ptr_array_new();
    g_ptr_array_add(advServiceUuids, SERVICE_ID);

    appData.bluetooth.adv = binc_advertisement_create();

    binc_advertisement_set_local_name(appData.bluetooth.adv, SERVICE_BT_NAME);
    binc_advertisement_set_services(appData.bluetooth.adv, advServiceUuids);
    g_ptr_array_free(advServiceUuids, TRUE);
    binc_adapter_start_advertising(adapter, appData.bluetooth.adv);

    Application* app = binc_create_application(adapter);
    appData.bluetooth.app = app;

    binc_application_add_service(app, SERVICE_ID);
    binc_application_add_characteristic(app, SERVICE_ID, CHAR_ID_MAIN, GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY);
    binc_application_add_characteristic(app, SERVICE_ID, CHAR_ID_FILTER, GATT_CHR_PROP_WRITE);

    binc_application_set_char_read_cb(app, &onCharRead);
    binc_application_set_char_write_cb(app, &onCharWrite);
    binc_application_set_char_start_notify_cb(app, &onCharStartNotify);
    binc_application_set_char_stop_notify_cb(app, &onCharStopNotify);

    binc_adapter_register_application(adapter, app);

    appData.isBluetoothWorkerRunning = TRUE;

    g_main_loop_run(mainLoop);

    g_main_loop_unref(mainLoop);

    g_message("Bluetooth worker shutting down");
    appData.isBluetoothWorkerRunning = FALSE;

    return NULL;
}