/*
 * botoeira_rx.c – recepção do estado da Botoeira na S3 (UART2, msg 0x040).
 */

#include "botoeira_rx.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define UART_NUM         UART_NUM_2
#define BOT_TX_PIN       15      // S3 TX2 -> Botoeira RX (GPIO1)
#define BOT_RX_PIN       16      // S3 RX2 <- Botoeira TX (GPIO0)
#define BAUD             115200
#define SOF              0xAA
#define MSG_ID_BOTOEIRA  0x040

static volatile uint16_t s_buttons = 0;
static volatile int8_t   s_vx = 0, s_vy = 0;

static uint8_t crc8(const uint8_t *d, int n)
{
    uint8_t c = 0;
    for (int i = 0; i < n; i++) c ^= d[i];
    return c;
}

static void rx_task(void *arg)
{
    while (1) {
        uint8_t b;
        if (uart_read_bytes(UART_NUM, &b, 1, pdMS_TO_TICKS(100)) <= 0 || b != SOF) continue;

        uint8_t hdr[3];
        if (uart_read_bytes(UART_NUM, hdr, 3, pdMS_TO_TICKS(20)) < 3) continue;
        uint16_t id  = hdr[0] | ((uint16_t)hdr[1] << 8);
        uint8_t  len = hdr[2];
        if (len > 8) continue;

        uint8_t tail[10];
        if (uart_read_bytes(UART_NUM, tail, len + 2, pdMS_TO_TICKS(20)) < len + 2) continue;

        uint8_t seq = tail[len], crc_recv = tail[len + 1];
        uint8_t buf[12];
        memcpy(buf, hdr, 3); memcpy(buf + 3, tail, len); buf[3 + len] = seq;
        if (crc8(buf, 4 + len) != crc_recv) continue;

        if (id == MSG_ID_BOTOEIRA && len >= 4) {
            s_buttons = tail[0] | ((uint16_t)tail[1] << 8);
            s_vx = (int8_t)tail[2];
            s_vy = (int8_t)tail[3];
        }
    }
}

void botoeira_rx_init(void)
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
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM, 256, 0, 0, NULL, 0));
    xTaskCreate(rx_task, "bot_rx", 4096, NULL, 5, NULL);
    ESP_LOGI("botoeira", "UART2 RX init (TX=GPIO%d RX=GPIO%d)", BOT_TX_PIN, BOT_RX_PIN);
}

void botoeira_get(uint16_t *buttons, int8_t *view_x, int8_t *view_y)
{
    *buttons = s_buttons;
    *view_x  = s_vx;
    *view_y  = s_vy;
}

// Mesmo parse do rx_task, mas a partir de um payload 0x040 já desembrulhado
// (o CAN entrega o payload sem SOF/CRC). Chamado pela on_module_msg da S3.
void botoeira_rx_feed(const uint8_t *payload, uint8_t len)
{
    if (len >= 4) {
        s_buttons = payload[0] | ((uint16_t)payload[1] << 8);
        s_vx = (int8_t)payload[2];
        s_vy = (int8_t)payload[3];
    }
}
