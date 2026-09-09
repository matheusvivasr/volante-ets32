#pragma once
#include <stdint.h>
#include "esp_err.h"

/*
 * can_bus – backend TWAI compartilhado por TODOS os nós do Volante DIY
 * (S3, Painel C3, Botoeira C3 e qualquer satélite futuro: pedais, câmbio...).
 *
 * Um SN65HVD230 por nó: TX/RX do ESP -> D/R do transceiver, CANH/CANL no par
 * trançado do barramento, GND comum, 120 ohm nas DUAS pontas físicas do
 * barramento (não um por nó). Ver LIGACOES.md §10 pro desenho e a receita de
 * como plugar um nó novo.
 *
 * Framing do projeto (igual nos 3 nós): payload útil <= 7 bytes, o CAN faz
 * SOF/CRC/ACK/arbitragem em hardware; o ÚLTIMO byte do frame (byte 8 do DLC)
 * é o contador de sequência, preenchido/lido por este componente.
 */

#define CAN_BUS_BITRATE_KBPS 500
#define CAN_BUS_MAX_PAYLOAD  7   // 8 (DLC máx) - 1 (byte de seq)

typedef struct {
    uint16_t id;
    uint8_t  len;                          // 0..CAN_BUS_MAX_PAYLOAD
    uint8_t  payload[CAN_BUS_MAX_PAYLOAD];
    uint8_t  seq;                          // TX: preenchido por can_bus_send(). RX: lido do frame.
} can_bus_msg_t;

typedef void (*can_bus_rx_cb_t)(const can_bus_msg_t *msg);

// Sobe o driver TWAI nos pinos indicados, a 500 kbps. Chamar uma vez por firmware,
// antes de can_bus_send()/can_bus_start_rx().
esp_err_t can_bus_init(int tx_gpio, int rx_gpio);

// Envia um frame (ID padrão de 11 bits). len > CAN_BUS_MAX_PAYLOAD é truncado.
// Bloqueia até 10ms se o TX estiver ocupado; não bloqueia esperando ACK.
esp_err_t can_bus_send(uint16_t id, const uint8_t *payload, uint8_t len);

// Sobe uma task (prioridade 5, stack 4096) que recebe frames e chama cb pra
// cada um. Nós que só transmitem (ex.: Botoeira) não precisam chamar isto.
void can_bus_start_rx(can_bus_rx_cb_t cb);
