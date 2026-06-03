#include "transport.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "transport";

// UART usada para comunicação com o módulo painel (C3)
#define TRANSPORT_UART_NUM  UART_NUM_1
#define TRANSPORT_TX_PIN    17
#define TRANSPORT_RX_PIN    18
#define TRANSPORT_BAUD      115200
#define TRANSPORT_BUF_SIZE  256

// Marcador de início de frame
#define FRAME_SOF 0xAA

static transport_rx_cb_t s_rx_cb = NULL;
static uint8_t s_seq_tx = 0;

// ─── CRC-8 simples (XOR acumulado) ───────────────────────────────────────────
static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) crc ^= data[i];
    return crc;
}

// ─── Task de recepção ─────────────────────────────────────────────────────────
static void rx_task(void *arg)
{
    // Frame: [SOF 1B][ID_LO 1B][ID_HI 1B][LEN 1B][PAYLOAD até 8B][SEQ 1B][CRC 1B]
    uint8_t buf[TRANSPORT_MAX_PAYLOAD + 6];

    while (1) {
        // Aguarda SOF
        uint8_t byte;
        int r = uart_read_bytes(TRANSPORT_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (r <= 0 || byte != FRAME_SOF) continue;

        // Lê cabeçalho: ID_LO, ID_HI, LEN
        uint8_t hdr[3];
        r = uart_read_bytes(TRANSPORT_UART_NUM, hdr, 3, pdMS_TO_TICKS(20));
        if (r < 3) continue;

        uint16_t id  = hdr[0] | ((uint16_t)hdr[1] << 8);
        uint8_t  len = hdr[2];
        if (len > TRANSPORT_MAX_PAYLOAD) continue;

        // Lê payload + seq + crc
        uint8_t tail[TRANSPORT_MAX_PAYLOAD + 2];
        r = uart_read_bytes(TRANSPORT_UART_NUM, tail, len + 2, pdMS_TO_TICKS(20));
        if (r < len + 2) continue;

        uint8_t seq      = tail[len];
        uint8_t crc_recv = tail[len + 1];

        // Valida CRC sobre [ID_LO, ID_HI, LEN, PAYLOAD..., SEQ]
        uint8_t crc_buf[3 + TRANSPORT_MAX_PAYLOAD + 1];
        crc_buf[0] = hdr[0]; crc_buf[1] = hdr[1]; crc_buf[2] = hdr[2];
        memcpy(&crc_buf[3], tail, len);
        crc_buf[3 + len] = seq;
        if (crc8(crc_buf, 4 + len) != crc_recv) {
            ESP_LOGW(TAG, "CRC error id=0x%03X", id);
            continue;
        }

        if (s_rx_cb) {
            transport_msg_t msg = {.id = id, .len = len, .seq = seq};
            memcpy(msg.payload, tail, len);
            s_rx_cb(&msg);
        }
    }
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void transport_init(transport_rx_cb_t rx_callback)
{
    s_rx_cb = rx_callback;

    uart_config_t cfg = {
        .baud_rate  = TRANSPORT_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(TRANSPORT_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(TRANSPORT_UART_NUM,
                                 TRANSPORT_TX_PIN, TRANSPORT_RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(TRANSPORT_UART_NUM,
                                        TRANSPORT_BUF_SIZE, TRANSPORT_BUF_SIZE,
                                        0, NULL, 0));

    xTaskCreate(rx_task, "transport_rx", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "UART transport init OK (TX=%d RX=%d baud=%d)",
             TRANSPORT_TX_PIN, TRANSPORT_RX_PIN, TRANSPORT_BAUD);
}

// ─── Envio ────────────────────────────────────────────────────────────────────
void transport_send(const transport_msg_t *msg)
{
    uint8_t frame[1 + 3 + TRANSPORT_MAX_PAYLOAD + 2]; // SOF+HDR+PAYLOAD+SEQ+CRC
    uint8_t idx = 0;

    frame[idx++] = FRAME_SOF;
    frame[idx++] = msg->id & 0xFF;
    frame[idx++] = (msg->id >> 8) & 0xFF;
    frame[idx++] = msg->len;
    memcpy(&frame[idx], msg->payload, msg->len);
    idx += msg->len;
    frame[idx++] = s_seq_tx++;

    // CRC sobre tudo exceto o SOF e o próprio CRC
    frame[idx] = crc8(&frame[1], idx - 1);
    idx++;

    uart_write_bytes(TRANSPORT_UART_NUM, (const char *)frame, idx);
}
