#pragma once
#include <stdint.h>

// IDs (iguais ao firmware S3 e ao Python)
#define MSG_TELEM_PAINEL 0x100
#define MSG_TELEM_LEDS   0x101
#define MSG_TELEM_NAV    0x102
#define MSG_HEARTBEAT    0x7FF

#define TRANSPORT_MAX_PAYLOAD 8

typedef struct {
    uint16_t id;
    uint8_t  len;
    uint8_t  payload[TRANSPORT_MAX_PAYLOAD];
    uint8_t  seq;
} transport_msg_t;

typedef void (*transport_rx_cb_t)(const transport_msg_t *msg);

// UART da S3 → C3 (placa C3-0.42): RX=GPIO1, TX=GPIO0.
// GPIO5/6 estão ocupados pelo OLED 0.42 embutido; por isso a UART saiu deles.
//   C3 GPIO1 (RX) <- S3 GPIO17 (TX)
//   C3 GPIO0 (TX) -> S3 GPIO18 (RX)
#define C3_RX_PIN 1
#define C3_TX_PIN 0

void transport_rx_init(transport_rx_cb_t cb);
