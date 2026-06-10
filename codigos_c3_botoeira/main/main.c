/*
 * main.c – Botoeira (ESP32-C3): lê 8 botões + joystick de visão e envia o
 * estado pra S3 (mensagem 0x040) por UART. On-change + heartbeat ~200ms.
 * A S3 mapeia o 0x040 em botões/eixos HID -> ETS2 (etapa 5).
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "transport_tx.h"
#include "inputs.h"
#include <stdlib.h>

static const char *TAG = "botoeira";

void app_main(void)
{
    ESP_LOGI(TAG, "=== Volante DIY - Botoeira C3 ===");
    transport_tx_init();
    inputs_init();

    inputs_state_t prev = { .buttons = 0xFFFF };   // força o 1º envio
    uint32_t tick = 0;

    while (1) {
        inputs_state_t st;
        inputs_read(&st);

        bool changed = (st.buttons != prev.buttons) ||
                       (abs(st.view_x - prev.view_x) > 2) ||
                       (abs(st.view_y - prev.view_y) > 2);

        // envia on-change OU a cada ~200ms (heartbeat: 10 * 20ms)
        if (changed || (tick % 10 == 0)) {
            uint8_t p[4] = {
                (uint8_t)(st.buttons & 0xFF),
                (uint8_t)((st.buttons >> 8) & 0xFF),
                (uint8_t)st.view_x,
                (uint8_t)st.view_y,
            };
            transport_tx_send(MSG_ID_BOTOEIRA, p, 4);
            prev = st;
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
