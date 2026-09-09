/*
 * transport_can.c – backend CAN da S3, sobre o componente compartilhado can_bus
 * (components/can_bus). Implementa A MESMA interface do transport.h
 * (transport_init / transport_send); main.c não muda.
 *
 * Pinos = os mesmos da antiga UART1 (que saiu de cena na migração p/ CAN).
 * Ligação física: SN65HVD230 no nó (TX/RX -> D/R do transceiver, CANH/CANL no
 * par trançado, GND comum, 120 ohm nas DUAS pontas físicas do barramento).
 * Ver LIGACOES.md §10.
 */

#include "transport.h"
#include "can_bus.h"
#include <string.h>

#define CAN_TX_PIN  17
#define CAN_RX_PIN  18

static transport_rx_cb_t s_rx_cb = NULL;

static void on_can_msg(const can_bus_msg_t *m)
{
    transport_msg_t msg = { .id = m->id, .len = m->len, .seq = m->seq };
    memcpy(msg.payload, m->payload, m->len);
    if (s_rx_cb) s_rx_cb(&msg);
}

void transport_init(transport_rx_cb_t rx_callback)
{
    s_rx_cb = rx_callback;
    ESP_ERROR_CHECK(can_bus_init(CAN_TX_PIN, CAN_RX_PIN));
    can_bus_start_rx(on_can_msg);
}

void transport_send(const transport_msg_t *msg)
{
    can_bus_send(msg->id, msg->payload, msg->len);
}
