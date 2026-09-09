#include "can_bus.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "can_bus";
static uint8_t s_seq_tx = 0;

esp_err_t can_bus_init(int tx_gpio, int rx_gpio)
{
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(tx_gpio, rx_gpio, TWAI_MODE_NORMAL);
    twai_timing_config_t  t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t err = twai_driver_install(&g, &t, &f);
    if (err != ESP_OK) return err;

    err = twai_start();
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "TWAI/CAN init OK (TX=GPIO%d RX=GPIO%d, %d kbps)", tx_gpio, rx_gpio, CAN_BUS_BITRATE_KBPS);
    return ESP_OK;
}

esp_err_t can_bus_send(uint16_t id, const uint8_t *payload, uint8_t len)
{
    if (len > CAN_BUS_MAX_PAYLOAD) len = CAN_BUS_MAX_PAYLOAD;

    twai_message_t m = {0};
    m.identifier = id;
    m.extd = 0;                       // ID padrão 11 bits
    memcpy(m.data, payload, len);
    m.data[len] = s_seq_tx++;         // seq = último byte
    m.data_length_code = len + 1;

    return twai_transmit(&m, pdMS_TO_TICKS(10));
}

static void rx_task(void *arg)
{
    can_bus_rx_cb_t cb = (can_bus_rx_cb_t)arg;

    while (1) {
        twai_message_t m;
        if (twai_receive(&m, portMAX_DELAY) != ESP_OK) continue;
        if (m.rtr || m.extd) continue;            // só data frames, ID 11 bits

        can_bus_msg_t msg = { .id = (uint16_t)m.identifier };
        if (m.data_length_code >= 1) {
            uint8_t n = m.data_length_code - 1;    // último byte = seq
            if (n > CAN_BUS_MAX_PAYLOAD) n = CAN_BUS_MAX_PAYLOAD;
            memcpy(msg.payload, m.data, n);
            msg.len = n;
            msg.seq = m.data[m.data_length_code - 1];
        }
        cb(&msg);
    }
}

void can_bus_start_rx(can_bus_rx_cb_t cb)
{
    xTaskCreate(rx_task, "can_rx", 4096, (void *)cb, 5, NULL);
}
