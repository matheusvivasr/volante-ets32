#include "transport_rx.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "transport_rx";
#define UART_NUM UART_NUM_1
#define BAUD     115200
#define BUF      256
#define SOF      0xAA

static transport_rx_cb_t s_cb;

static uint8_t crc8(const uint8_t *d, size_t n)
{
    uint8_t c = 0;
    for (size_t i = 0; i < n; i++) c ^= d[i];
    return c;
}

static void rx_task(void *arg)
{
    while (1) {
        uint8_t b;
        if (uart_read_bytes(UART_NUM, &b, 1, pdMS_TO_TICKS(200)) <= 0 || b != SOF)
            continue;

        uint8_t hdr[3];
        if (uart_read_bytes(UART_NUM, hdr, 3, pdMS_TO_TICKS(20)) < 3) continue;

        uint16_t id = hdr[0] | ((uint16_t)hdr[1] << 8);
        uint8_t  len = hdr[2];
        if (len > TRANSPORT_MAX_PAYLOAD) continue;

        uint8_t tail[TRANSPORT_MAX_PAYLOAD + 2];
        if (uart_read_bytes(UART_NUM, tail, len + 2, pdMS_TO_TICKS(20)) < len + 2)
            continue;

        uint8_t seq      = tail[len];
        uint8_t crc_recv = tail[len + 1];

        // valida CRC
        uint8_t buf[3 + TRANSPORT_MAX_PAYLOAD + 1];
        memcpy(buf, hdr, 3);
        memcpy(buf + 3, tail, len);
        buf[3 + len] = seq;
        if (crc8(buf, 4 + len) != crc_recv) {
            ESP_LOGW(TAG, "CRC err id=0x%03X", id);
            continue;
        }

        if (s_cb) {
            transport_msg_t m = {.id = id, .len = len, .seq = seq};
            memcpy(m.payload, tail, len);
            s_cb(&m);
        }
    }
}

void transport_rx_init(transport_rx_cb_t cb)
{
    s_cb = cb;
    uart_config_t cfg = {
        .baud_rate = BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM, C3_TX_PIN, C3_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, BUF, BUF, 0, NULL, 0));
    xTaskCreate(rx_task, "transport_rx", 3072, NULL, 6, NULL);
    ESP_LOGI(TAG, "UART RX OK (RX=GPIO%d TX=GPIO%d)", C3_RX_PIN, C3_TX_PIN);
}
