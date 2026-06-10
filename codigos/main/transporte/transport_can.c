/*
 * transport_can.c – backend CAN (TWAI) do transporte da S3.  *** NAO TESTADO ***
 *
 * Implementa A MESMA interface do transport.h (transport_init / transport_send),
 * trocando UART por TWAI. O main.c nao muda. GUARDADO p/ quando os transceivers
 * SN65HVD230 chegarem (ainda nao adquiridos em 2026-06-10).
 *
 * COMO ATIVAR (migracao UART->CAN):
 *   1. No main/CMakeLists.txt: trocar "transport.c" por "transport_can.c".
 *   2. Ligar um SN65HVD230 por no: ESP TWAI_TX/RX -> D/R do transceiver,
 *      CANH/CANL no par trancado, GND comum, 120 ohm em CADA ponta do barramento.
 *   3. Mesmos IDs do UART. Compilar e testar (este arquivo nunca foi compilado).
 *
 * Diferencas p/ o UART: o CAN faz framing/CRC/ACK/arbitragem em HARDWARE, entao
 * some o SOF/LEN/CRC8. O contador de sequencia vai como ULTIMO byte do payload
 * CAN (payload util <= 7 bytes; todas as mensagens do projeto cabem).
 */

#include "transport.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "transport_can";

// Pinos pro transceiver (os mesmos da UART1, que sai de cena na migracao).
#define CAN_TX_PIN  17
#define CAN_RX_PIN  18

static transport_rx_cb_t s_rx_cb = NULL;
static uint8_t s_seq_tx = 0;

static void rx_task(void *arg)
{
    while (1) {
        twai_message_t m;
        if (twai_receive(&m, portMAX_DELAY) != ESP_OK) continue;
        if (m.rtr || m.extd) continue;            // so data frames, ID 11 bits

        transport_msg_t msg = {0};
        msg.id = (uint16_t)m.identifier;
        if (m.data_length_code >= 1) {
            uint8_t n = m.data_length_code - 1;    // ultimo byte = seq
            if (n > TRANSPORT_MAX_PAYLOAD) n = TRANSPORT_MAX_PAYLOAD;
            memcpy(msg.payload, m.data, n);
            msg.len = n;
            msg.seq = m.data[m.data_length_code - 1];
        }
        if (s_rx_cb) s_rx_cb(&msg);
    }
}

void transport_init(transport_rx_cb_t rx_callback)
{
    s_rx_cb = rx_callback;

    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t  t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g, &t, &f));
    ESP_ERROR_CHECK(twai_start());

    xTaskCreate(rx_task, "can_rx", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "TWAI/CAN transport init OK (TX=%d RX=%d, 500kbps)", CAN_TX_PIN, CAN_RX_PIN);
}

void transport_send(const transport_msg_t *msg)
{
    twai_message_t m = {0};
    m.identifier = msg->id;
    m.extd = 0;                                    // ID padrao 11 bits

    uint8_t n = msg->len;
    if (n > TRANSPORT_MAX_PAYLOAD - 1) n = TRANSPORT_MAX_PAYLOAD - 1;  // reserva 1 p/ o seq
    memcpy(m.data, msg->payload, n);
    m.data[n] = s_seq_tx++;                         // seq = ultimo byte
    m.data_length_code = n + 1;

    twai_transmit(&m, pdMS_TO_TICKS(10));
}
