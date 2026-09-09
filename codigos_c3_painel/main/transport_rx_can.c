/*
 * transport_rx_can.c – backend CAN do painel C3, sobre o componente
 * compartilhado can_bus (components/can_bus). Implementa A MESMA interface
 * do transport_rx.h (transport_rx_init); só recepção (o painel só consome).
 *
 * Pinos = os mesmos da antiga UART1 da C3 (que saiu de cena na migração p/
 * CAN). Ligação física: SN65HVD230 no nó, CANH/CANL no barramento, GND
 * comum, 120 ohm nas DUAS pontas físicas do barramento. Ver LIGACOES.md §10.
 */

#include "transport_rx.h"
#include "can_bus.h"
#include <string.h>

#define CAN_TX_PIN  0
#define CAN_RX_PIN  1

static transport_rx_cb_t s_cb = NULL;

static void on_can_msg(const can_bus_msg_t *m)
{
    transport_msg_t msg = { .id = m->id, .len = m->len, .seq = m->seq };
    memcpy(msg.payload, m->payload, m->len);
    if (s_cb) s_cb(&msg);
}

void transport_rx_init(transport_rx_cb_t cb)
{
    s_cb = cb;
    ESP_ERROR_CHECK(can_bus_init(CAN_TX_PIN, CAN_RX_PIN));
    can_bus_start_rx(on_can_msg);
}
