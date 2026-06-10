/*
 * transport_rx_can.c – backend CAN (TWAI) do painel C3.  *** NAO TESTADO ***
 *
 * Implementa A MESMA interface do transport_rx.h (transport_rx_init), trocando
 * UART por TWAI. So recepcao (o painel so consome). GUARDADO p/ quando os
 * transceivers SN65HVD230 chegarem (ainda nao adquiridos em 2026-06-10).
 *
 * COMO ATIVAR (migracao UART->CAN):
 *   1. No main/CMakeLists.txt: trocar "transport_rx.c" por "transport_rx_can.c".
 *   2. Ligar um SN65HVD230: C3 GPIO0=TWAI_TX, GPIO1=TWAI_RX -> D/R do transceiver;
 *      CANH/CANL no barramento, GND comum, 120 ohm nas duas pontas.
 *   3. Compilar e testar (este arquivo nunca foi compilado).
 *
 * O CAN faz framing/CRC em hardware; o seq vem como ULTIMO byte do payload CAN.
 */

#include "transport_rx.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "transport_rx_can";

// Pinos pro transceiver (os mesmos da UART1 da C3, que sai de cena na migracao).
#define CAN_TX_PIN  0
#define CAN_RX_PIN  1

static transport_rx_cb_t s_cb = NULL;

static void rx_task(void *arg)
{
    while (1) {
        twai_message_t m;
        if (twai_receive(&m, portMAX_DELAY) != ESP_OK) continue;
        if (m.rtr || m.extd) continue;

        transport_msg_t msg = {0};
        msg.id = (uint16_t)m.identifier;
        if (m.data_length_code >= 1) {
            uint8_t n = m.data_length_code - 1;
            if (n > TRANSPORT_MAX_PAYLOAD) n = TRANSPORT_MAX_PAYLOAD;
            memcpy(msg.payload, m.data, n);
            msg.len = n;
            msg.seq = m.data[m.data_length_code - 1];
        }
        if (s_cb) s_cb(&msg);
    }
}

void transport_rx_init(transport_rx_cb_t cb)
{
    s_cb = cb;

    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t  t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g, &t, &f));
    ESP_ERROR_CHECK(twai_start());

    xTaskCreate(rx_task, "can_rx", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "TWAI/CAN RX init OK (TX=GPIO%d RX=GPIO%d, 500kbps)", CAN_TX_PIN, CAN_RX_PIN);
}
