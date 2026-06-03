#pragma once

#include <stdint.h>
#include <stddef.h>

// ─── IDs de mensagem (iguais em UART e CAN) ───────────────────────────────────
#define MSG_ID_PEDAIS       0x020
#define MSG_ID_CAMBIO       0x030
#define MSG_ID_BOTOEIRA     0x040
#define MSG_ID_TELEM_PAINEL 0x100
#define MSG_ID_TELEM_LEDS   0x101
#define MSG_ID_TELEM_NAV    0x102
#define MSG_ID_HEARTBEAT    0x7FF

// ─── Frame UART: [ID 1B][LEN 1B][PAYLOAD até 8B][SEQ 1B][CRC 1B] ─────────────
#define TRANSPORT_MAX_PAYLOAD 8

typedef struct {
    uint16_t id;
    uint8_t  len;
    uint8_t  payload[TRANSPORT_MAX_PAYLOAD];
    uint8_t  seq;
} transport_msg_t;

// Callback chamado quando uma mensagem válida chega
typedef void (*transport_rx_cb_t)(const transport_msg_t *msg);

// ─── API pública ──────────────────────────────────────────────────────────────
void transport_init(transport_rx_cb_t rx_callback);
void transport_send(const transport_msg_t *msg);
