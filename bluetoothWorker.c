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

#define SERVICE_UUID "00001FF8-0000-1000-8000-00805F9B34FB" 
#define CHAR_UUID_MAIN "00000001-0000-1000-8000-00805F9B34FB"
#define CHAR_UUID_FILTER "00000002-0000-1000-8000-00805F9B34FB"

GMainLoop* loop = NULL;

GDBusConnection* dbusConnection = NULL;
Adapter* default_adapter = NULL;
Advertisement* advertisement = NULL;
Application* app = NULL;

void onPoweredStateChanged(Adapter* adapter, gboolean state) {
    g_message("powered '%s' (%s)", state ? "on" : "off", binc_adapter_get_path(adapter));
    if (state == TRUE) return;

    g_message("powering adapter up...");
    binc_adapter_power_on(default_adapter);
}

void onCentralStateChanged(Adapter* adapter, Device* device) {
    g_message("remote central %s is %s", binc_device_get_address(device), binc_device_get_connection_state_name(device));
    ConnectionState state = binc_device_get_connection_state(device);
    if (state == CONNECTED)  binc_adapter_stop_advertising(adapter, advertisement);
    else if (state == DISCONNECTED) binc_adapter_start_advertising(adapter, advertisement);
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
    return NULL;
}

void onCharStartNotify(const Application* application, const char* service_uuid, const char* char_uuid) {
    g_message("on start notify");

    if (g_str_equal(service_uuid, SERVICE_UUID) && g_str_equal(char_uuid, CHAR_UUID_MAIN)) {
        const guint8 bytes[] = { 0x06, 0x6A, 0x01, 0x00, 0xff, 0xe6, 0x07, 0x03, 0x03, 0x10, 0x04, 0x00, 0x01 };
        GByteArray* byteArray = g_byte_array_sized_new(sizeof(bytes));
        g_byte_array_append(byteArray, bytes, sizeof(bytes));
        binc_application_notify(application, service_uuid, char_uuid, byteArray);
        g_byte_array_free(byteArray, TRUE);
    }
}

void onChartStopNotify(const Application* application, const char* service_uuid, const char* char_uuid) {
    g_message("on stop notify");
}

static void stopBluetoothService() {
    if (app != NULL) {
        binc_adapter_unregister_application(default_adapter, app);
        binc_application_free(app);
        app = NULL;
    }

    if (advertisement != NULL) {
        binc_adapter_stop_advertising(default_adapter, advertisement);
        binc_advertisement_free(advertisement);
    }

    if (default_adapter != NULL) {
        binc_adapter_free(default_adapter);
        default_adapter = NULL;
    }

    if (dbusConnection != NULL) {
        g_dbus_connection_close_sync(dbusConnection, NULL, NULL);
        g_object_unref(dbusConnection);
    }
}

static gpointer bluetoothWorkerLoop(gpointer data) {
    // Get a DBus connection
    dbusConnection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);

    // Get the default default_adapter
    default_adapter = binc_adapter_get_default(dbusConnection);

    if (default_adapter == NULL) {
        g_error("No default_adapter found");
        return NULL;
    }

    g_message("using default_adapter '%s'", binc_adapter_get_path(default_adapter));

    // Make sure the adapter is on
    binc_adapter_set_powered_state_cb(default_adapter, &onPoweredStateChanged);
    if (!binc_adapter_get_powered_state(default_adapter)) binc_adapter_power_on(default_adapter);


    // Setup remote central connection state
    binc_adapter_set_remote_central_cb(default_adapter, &onCentralStateChanged);

    // Setup advertisement
    GPtrArray* adv_service_uuids = g_ptr_array_new();
    g_ptr_array_add(adv_service_uuids, SERVICE_UUID);

    advertisement = binc_advertisement_create();
    binc_advertisement_set_local_name(advertisement, "BINC");
    binc_advertisement_set_services(advertisement, adv_service_uuids);
    g_ptr_array_free(adv_service_uuids, TRUE);
    binc_adapter_start_advertising(default_adapter, advertisement);

    // Start application
    app = binc_create_application(default_adapter);
    binc_application_add_service(app, SERVICE_UUID);

    binc_application_add_characteristic(
        app,
        SERVICE_UUID,
        CHAR_UUID_MAIN,
        GATT_CHR_PROP_READ | GATT_CHR_PROP_NOTIFY);

    binc_application_add_characteristic(
        app,
        SERVICE_UUID,
        CHAR_UUID_FILTER,
        GATT_CHR_PROP_WRITE);

    binc_application_set_char_read_cb(app, &onCharRead);
    binc_application_set_char_write_cb(app, &onCharWrite);

    binc_application_set_char_start_notify_cb(app, &onCharStartNotify);
    binc_application_set_char_stop_notify_cb(app, &onChartStopNotify);

    binc_adapter_register_application(default_adapter, app);

    return NULL;
}