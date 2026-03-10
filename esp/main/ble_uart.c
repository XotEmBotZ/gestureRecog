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
static const ble_uuid128_t nus_svc_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

static const ble_uuid128_t nus_rx_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

static const ble_uuid128_t nus_tx_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static uint16_t nus_tx_handle;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint8_t ble_addr_type;

static int ble_uart_gap_event(struct ble_gap_event *event, void *arg);
static void ble_uart_advertise(void);

extern QueueHandle_t ble_rx_queue;

static int nus_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ble_uuid_cmp(ctxt->chr->uuid, &nus_rx_chr_uuid.u) == 0) {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
            char rx_data[64];
            uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
            if (len > sizeof(rx_data) - 1) len = sizeof(rx_data) - 1;
            
            int rc = ble_hs_mbuf_to_flat(ctxt->om, rx_data, len, NULL);
            if (rc == 0) {
                rx_data[len] = '\0';
                ESP_LOGI(TAG, "BLE RX: %s", rx_data);
                if (ble_rx_queue != NULL) {
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

static void ble_uart_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    memset(&fields, 0, sizeof(fields));

    // PACKET: Flags + Name only for Legacy simplicity
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)"GLOVE";
    fields.name_len = 5;
    fields.name_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    
    // Explicitly set channel map (0x07 = channels 37, 38, 39)
    adv_params.channel_map = 0x07;
    
    // Interval: 100ms
    adv_params.itvl_min = 160; 
    adv_params.itvl_max = 160;

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_uart_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising started: GLOVE (Legacy Mode)");
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
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect; reason=%d", event->disconnect.reason);
        conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ble_uart_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "adv complete; reason=%d", event->adv_complete.reason);
        ble_uart_advertise();
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update; conn_handle=%d mtu=%d", event->mtu.conn_handle, event->mtu.value);
        break;
    }
    return 0;
}

static void ble_uart_on_sync(void) {
    int rc;
    ESP_LOGI(TAG, "NimBLE Host/Controller Synchronized");
    
    // Ensure we have a public address (Type 0)
    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);
    
    uint8_t addr[6];
    ble_hs_id_copy_addr(ble_addr_type, addr, NULL);
    ESP_LOGI(TAG, "Device Address: %02X:%02X:%02X:%02X:%02X:%02X (Type: %d)",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0], ble_addr_type);

    ble_uart_advertise();
}

static void ble_uart_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_uart_init(void) {
    ESP_LOGI(TAG, "Initializing NimBLE Stack...");
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed; ret=%d", ret);
        return ret;
    }

    ble_hs_cfg.sync_cb = ble_uart_on_sync;
    ble_hs_cfg.reset_cb = NULL;
    ble_hs_cfg.store_status_cb = NULL;
    
    // Open connection, no security
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_sc = 0;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(nus_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed; rc=%d", rc);
    }
    rc = ble_gatts_add_svcs(nus_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed; rc=%d", rc);
    }
    
    ble_svc_gap_device_name_set("GLOVE");

    ESP_LOGI(TAG, "Starting NimBLE Host Task...");
    nimble_port_freertos_init(ble_uart_host_task);
    return ESP_OK;
}

int ble_printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        return ble_uart_send((uint8_t *)buf, (uint16_t)len);
    }
    return 0;
}

int ble_uart_send(const uint8_t *data, uint16_t len) {
    if (conn_handle == BLE_HS_CONN_HANDLE_NONE) return 0;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return 0;
    int rc = ble_gatts_notify_custom(conn_handle, nus_tx_handle, om);
    return (rc == 0) ? len : 0;
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
        printf("%s", buf);
        if (ble_uart_is_connected()) {
            ble_uart_send((uint8_t *)buf, (uint16_t)len);
        }
    }
}
