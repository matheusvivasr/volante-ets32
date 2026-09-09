/*
 * transport_tx_can.c – backend CAN da Botoeira, sobre o componente
 * compartilhado can_bus (components/can_bus). Implementa A MESMA interface
 * do transport_tx.h (transport_tx_init / transport_tx_send); main.c não muda.
 *
 * Pinos = os mesmos da antiga UART1 da C3 (que saiu de cena na migração p/
 * CAN). Ligação física: SN65HVD230 no nó, CANH/CANL no barramento, GND
 * comum, 120 ohm nas DUAS pontas físicas do barramento. Ver LIGACOES.md §10.
 * Sem ACK (S3 pode estar fora do bus) -> só timeout no send.
 */

#include "transport_tx.h"
#include "can_bus.h"

#define CAN_TX_PIN  BOT_TX_PIN   // GPIO0
#define CAN_RX_PIN  BOT_RX_PIN   // GPIO1

void transport_tx_init(void)
{
    ESP_ERROR_CHECK(can_bus_init(CAN_TX_PIN, CAN_RX_PIN));
}

void transport_tx_send(uint16_t id, const uint8_t *payload, uint8_t len)
{
    can_bus_send(id, payload, len);
}
