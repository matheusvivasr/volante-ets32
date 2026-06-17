/*
 * transport_tx_can.c – backend CAN (TWAI) de envio da Botoeira.
 *
 * Implementa A MESMA interface do transport_tx.h (transport_tx_init /
 * transport_tx_send), trocando UART por TWAI. main.c nao muda.
 *
 * Mesmo framing CAN do resto do projeto (transport_can.c da S3): o CAN faz
 * SOF/CRC/ACK/arbitragem em hardware; o contador de sequencia vai como ULTIMO
 * byte do payload CAN (payload util <= 7 bytes; o 0x040 usa 4).
 *
 * Pinos = os mesmos da UART1 da C3 (saem de cena na migracao):
 *   C3 GPIO0 = TWAI_TX -> CTX do SN65HVD230
 *   C3 GPIO1 = TWAI_RX -> CRX do SN65HVD230
 */

#include "transport_tx.h"
#include "driver/twai.h"
#include "esp_log.h"
#include <string.h>

#define CAN_TX_PIN  BOT_TX_PIN   // GPIO0
#define CAN_RX_PIN  BOT_RX_PIN   // GPIO1

static uint8_t s_seq = 0;

void transport_tx_init(void)
{
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
    twai_timing_config_t  t = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t  f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g, &t, &f));
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI("tx", "TWAI/CAN TX init (TX=GPIO%d RX=GPIO%d, 500kbps)", CAN_TX_PIN, CAN_RX_PIN);
}

void transport_tx_send(uint16_t id, const uint8_t *payload, uint8_t len)
{
    if (len > 7) len = 7;                  // reserva 1 byte p/ o seq (CAN <= 8)
    twai_message_t m = {0};
    m.identifier = id;
    m.extd = 0;                            // ID padrao 11 bits
    memcpy(m.data, payload, len);
    m.data[len] = s_seq++;                 // seq = ultimo byte (igual S3/painel)
    m.data_length_code = len + 1;

    twai_transmit(&m, pdMS_TO_TICKS(10));  // sem ACK (S3 fora do bus) -> so timeout
}
