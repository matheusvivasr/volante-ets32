#pragma once
#include <stdint.h>

/*
 * transport_tx.h – envio de mensagens da Botoeira p/ a S3 (mesmo framing do projeto).
 * Frame: [SOF 0xAA][ID_LO][ID_HI][LEN][PAYLOAD<=8][SEQ][CRC8 (XOR)].
 *
 * Enlace: UART1 da C3 -> UART2 da S3. C3 GPIO0=TX, GPIO1=RX.
 *   C3 GPIO0 (TX) -> S3 GPIO16 (RX2)
 *   C3 GPIO1 (RX) <- S3 GPIO15 (TX2)
 */

#define MSG_ID_BOTOEIRA  0x040

#define BOT_TX_PIN  0
#define BOT_RX_PIN  1

void transport_tx_init(void);
void transport_tx_send(uint16_t id, const uint8_t *payload, uint8_t len);
