#ifndef BLE_UART_H
#define BLE_UART_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize BLE UART Service (NUS)
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ble_uart_init(void);

/**
 * @brief Send data over BLE UART
 * 
 * @param data Pointer to data to send
 * @param len Length of data
 * @return int Number of bytes sent
 */
int ble_uart_send(const uint8_t *data, uint16_t len);

/**
 * @brief Printf-style function for BLE UART
 * 
 * @param fmt Format string
 * @param ... Arguments
 * @return int Number of bytes sent
 */
int ble_printf(const char *fmt, ...);

/**
 * @brief Check if a BLE client is connected
 * 
 * @return true if connected, false otherwise
 */
bool ble_uart_is_connected(void);

/**
 * @brief Print to both console and BLE UART
 */
void telemetry_printf(const char *fmt, ...);

#endif // BLE_UART_H
