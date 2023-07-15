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
#include "workerData.c"

#define SERVICE_BT_NAME "KnurDash BLE" 
#define SERVICE_UUID "00001ff8-0000-1000-8000-00805f9b34fb" 
#define CHAR_UUID_MAIN "00000001-0000-1000-8000-00805f9b34fb"
#define CHAR_UUID_FILTER "00000002-0000-1000-8000-00805f9b34fb"

#define BLUETOOTH_WORKER_SHUTDOWN_LOOP_INTERVAL 500

Advertisement* knurDashAdv;

void onPoweredStateChanged(Adapter* adapter, gboolean state) {
    g_message("BT adapter powered '%s' (%s)", state ? "on" : "off", binc_adapter_get_path(adapter));
    if (state == TRUE) return;

    g_message("Powering up BT adapter (%s)", binc_adapter_get_path(adapter));
    binc_adapter_power_on(adapter);
}

void onCentralStateChanged(Adapter* adapter, Device* device) {
    g_message("Remote central %s is %s", binc_device_get_address(device), binc_device_get_connection_state_name(device));

    ConnectionState state = binc_device_get_connection_state(device);
    if (state == CONNECTED)  binc_adapter_stop_advertising(adapter, knurDashAdv);
    else if (state == DISCONNECTED) binc_adapter_start_advertising(adapter, knurDashAdv);
}

const char* onCharRead(const Application* application, const char* address, const char* service_uuid, const char* char_uuid) {
    if (g_str_equal(service_uuid, SERVICE_UUID) && g_str_equal(char_uuid, CHAR_UUID_MAIN)) {
        const guint8 bytes[] = { 0x06, 0x6f, 0x01, 0x00, 0xff, 0xe6, 0x07, 0x03, 0x03, 0x10, 0x04, 0x00, 0x01 };
        GByteArray* byteArray = g_byte_array_sized_new(sizeof(bytes));
        g_byte_array_append(byteArray, bytes, sizeof(bytes));
        binc_application_set_char_value(application, service_uuid, char_uuid, byteArray);
        return NULL;
    }
    return BLUEZ_ERROR_REJECTED;
}

const char* onCharWrite(const Application* application, const char* address, const char* service_uuid, const char* char_uuid, GByteArray* byteArray) {
    g_message("Write received on: %s, length:%d, values:%x,%x,%x  ", char_uuid, byteArray->len, byteArray->data[0], byteArray->data[1], byteArray->data[2]);
}

gboolean stopBtWorker() {

    if (workerData.requestShutdown == FALSE) return G_SOURCE_CONTINUE;

    GMainContext* context = g_main_loop_get_context(workerData.bluetoothData.mainLoop);
    while (g_main_context_pending(context)) g_main_context_iteration(context, TRUE);

    Adapter* adapter = workerData.bluetoothData.adapter;

    if (workerData.bluetoothData.application != NULL) {
        binc_adapter_unregister_application(adapter, workerData.bluetoothData.application);
        binc_application_free(workerData.bluetoothData.application);
        workerData.bluetoothData.application = NULL;
    }

    if (knurDashAdv != NULL) {
        binc_adapter_stop_advertising(adapter, knurDashAdv);
        binc_advertisement_free(knurDashAdv);
    }

    if (adapter != NULL) {
        binc_adapter_free(adapter);
        workerData.bluetoothData.adapter = NULL;
    }

    if (workerData.bluetoothData.connection != NULL) {
        g_dbus_connection_close_sync(workerData.bluetoothData.connection, NULL, NULL);
        g_object_unref(workerData.bluetoothData.connection);
    }

    g_main_loop_quit(workerData.bluetoothData.mainLoop);

    g_message("Bluetooth Worker stopBtWorker done");

    return G_SOURCE_REMOVE;
}

gpointer bluetoothWorkerLoop() {
#ifdef NDEBUG
    log_set_level(LOG_WARN);
#endif

    GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
    workerData.bluetoothData.connection = connection;

    Adapter* adapter = binc_adapter_get_default(connection);
    workerData.bluetoothData.adapter = adapter;

    if (adapter == NULL) {
        g_error("No adapter found");
        return NULL;
    }

    g_message("Bluetooth worker starting, adapter '%s'", binc_adapter_get_path(adapter));

    binc_adapter_set_powered_state_cb(adapter, &onPoweredStateChanged);
    if (!binc_adapter_get_powered_state(adapter)) binc_adapter_power_on(adapter);

    binc_adapter_set_remote_central_cb(adapter, &onCentralStateChanged);

    GPtrArray* advServiceUuids = g_ptr_array_new();
    g_ptr_array_add(advServiceUuids, SERVICE_UUID);

    knurDashAdv = binc_advertisement_create();

    binc_advertisement_set_local_name(knurDashAdv, SERVICE_BT_NAME);
    binc_advertisement_set_services(knurDashAdv, advServiceUuids);
    g_ptr_array_free(advServiceUuids, TRUE);
    binc_adapter_start_advertising(adapter, knurDashAdv);

    Application* application = binc_create_application(adapter);
    workerData.bluetoothData.application = application;

    binc_application_add_service(application, SERVICE_UUID);

    binc_application_add_characteristic(
        application,
        SERVICE_UUID,
        CHAR_UUID_MAIN,
        GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY);

    binc_application_add_characteristic(
        application,
        SERVICE_UUID,
        CHAR_UUID_FILTER,
        GATT_CHR_PROP_WRITE);

    binc_application_set_char_read_cb(application, &onCharRead);
    binc_application_set_char_write_cb(application, &onCharWrite);

    binc_adapter_register_application(adapter, application);

    GMainContext* workerContext = g_main_context_new();
    g_main_context_push_thread_default(workerContext);
    GMainLoop* mainLoop = g_main_loop_new(workerContext, FALSE);
    workerData.bluetoothData.mainLoop = mainLoop;

    GSource* stopBtWorkerSource = g_timeout_source_new(BLUETOOTH_WORKER_SHUTDOWN_LOOP_INTERVAL);
    g_source_set_callback(stopBtWorkerSource, stopBtWorker, NULL, NULL);
    g_source_attach(stopBtWorkerSource, workerContext);
    g_source_unref(stopBtWorkerSource);

    workerData.isBluetoothWorkerRunning = TRUE;

    g_main_loop_run(mainLoop);

    g_main_loop_unref(mainLoop);

    g_message("Bluetooth worker shutting down");

    return NULL;
}