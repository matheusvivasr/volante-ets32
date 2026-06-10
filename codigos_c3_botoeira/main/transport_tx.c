#include "transport_tx.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

#define UART_NUM   UART_NUM_1
#define BAUD       115200
#define SOF        0xAA

static uint8_t s_seq = 0;

static uint8_t crc8(const uint8_t *d, int n)
{
    uint8_t c = 0;
    for (int i = 0; i < n; i++) c ^= d[i];
    return c;
}

void transport_tx_init(void)
{
    uart_config_t cfg = {
        .baud_rate = BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, BOT_TX_PIN, BOT_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, 256, 256, 0, NULL, 0));
    ESP_LOGI("tx", "UART TX init (TX=GPIO%d RX=GPIO%d, %d baud)", BOT_TX_PIN, BOT_RX_PIN, BAUD);
}

void transport_tx_send(uint16_t id, const uint8_t *payload, uint8_t len)
{
    if (len > 8) len = 8;
    uint8_t f[1 + 3 + 8 + 2];
    int i = 0;
    f[i++] = SOF;
    f[i++] = id & 0xFF;
    f[i++] = (id >> 8) & 0xFF;
    f[i++] = len;
    memcpy(&f[i], payload, len);
    i += len;
    f[i++] = s_seq++;
    f[i] = crc8(&f[1], i - 1);   // CRC sobre ID+LEN+PAYLOAD+SEQ
    i++;
    uart_write_bytes(UART_NUM, (const char *)f, i);
}
