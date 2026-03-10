#include "ble_uart.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_UART";

// Nordic UART Service (NUS) UUIDs
// 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t nus_svc_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

// RX Characteristic (Write) - 6E400002-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t nus_rx_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

// TX Characteristic (Notify) - 6E400003-B5A3-F393-E0A9-E50E24DCCA9E
static const ble_uuid128_t nus_tx_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static uint16_t nus_tx_handle;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint8_t ble_addr_type;

// External queue for console commands (defined in main.c or here)
extern QueueHandle_t ble_rx_queue;

static int nus_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def nus_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &nus_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &nus_rx_chr_uuid.u,
                .access_cb = nus_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &nus_tx_chr_uuid.u,
                .access_cb = nus_access_cb,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &nus_tx_handle,
            },
            {0},
        },
    },
    {0},
};

static int nus_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ble_uuid_cmp(ctxt->chr->uuid, &nus_rx_chr_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            char rx_data[64];
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len > sizeof(rx_data) - 1) len = sizeof(rx_data) - 1;
            
            int rc = ble_hs_mbuf_to_flat(ctxt->om, rx_data, len, NULL);
            if (rc == 0) {
                rx_data[len] = '\0';
                ESP_LOGD(TAG, "BLE RX: %s", rx_data);
                if (ble_rx_queue != NULL) {
                    // Send each line to the queue if needed, or just the whole block
                    // For simplicity, we assume one command per write or handle it in main
                    char *q_msg = strdup(rx_data);
                    if (xQueueSend(ble_rx_queue, &q_msg, 0) != pdTRUE) {
                        free(q_msg);
                    }
                }
            }
            return 0;
        }
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static void ble_uart_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));
    fields.name = (uint8_t *)"ESP32-S3-OSSIC";
    fields.name_len = strlen((char *)fields.name);
    fields.name_is_complete = 1;
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
    }
}

static int ble_uart_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connected; status=%d", event->connect.status);
        if (event->connect.status == 0) {
            conn_handle = event->connect.conn_handle;
        } else {
            ble_uart_advertise();
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect; reason=%d", event->disconnect.reason);
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_uart_advertise();
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "adv complete");
        ble_uart_advertise();
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update; conn_handle=%d mtu=%d", event->mtu.conn_handle, event->mtu.value);
        return 0;
    }
    return 0;
}

static void ble_uart_on_sync(void) {
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);
    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);
    ble_uart_advertise();
}

static void ble_uart_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_uart_init(void) {
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init nimble %d", ret);
        return ret;
    }

    ble_hs_cfg.sync_cb = ble_uart_on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    int rc = ble_gatts_count_cfg(nus_svcs);
    if (rc != 0) return ESP_FAIL;

    rc = ble_gatts_add_svcs(nus_svcs);
    if (rc != 0) return ESP_FAIL;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    rc = ble_svc_gap_device_name_set("ESP32-S3-OSSIC");
    if (rc != 0) return ESP_FAIL;

    nimble_port_freertos_init(ble_uart_host_task);
    
    // Set default GAP event handler
    ble_gap_event_set_default(ble_uart_gap_event, NULL);

    return ESP_OK;
}

int ble_uart_send(const uint8_t *data, uint16_t len) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return 0;

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return 0;

    int rc = ble_gattc_notify_custom(conn_handle, nus_tx_handle, om);
    return (rc == 0) ? len : 0;
}

int ble_printf(const char *fmt, ...) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return 0;

    char buf[128];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        if (len >= sizeof(buf)) len = sizeof(buf) - 1;
        return ble_uart_send((uint8_t *)buf, len);
    }
    return 0;
}

bool ble_uart_is_connected(void) {
    return conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

void telemetry_printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        // Print to local console (USB)
        printf("%s", buf);
        // Send to BLE if connected
        if (ble_uart_is_connected()) {
            ble_uart_send((uint8_t *)buf, (uint16_t)len);
        }
    }
}
