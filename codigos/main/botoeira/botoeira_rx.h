#pragma once
#include <stdint.h>

/*
 * botoeira_rx.h – S3 recebe o estado da Botoeira (msg 0x040).
 *
 * ATIVO desde 2026-09-09: chega pelo barramento CAN (transport_can.c ->
 * on_module_msg -> botoeira_rx_feed()), não mais por UART2 dedicada.
 * botoeira_rx_init()/rx_task (UART2, GPIO15/16) ficam no .c como código morto,
 * fora de uso — ninguém chama botoeira_rx_init() em main.c.
 * Payload 0x040 (4 bytes): [botoes uint16][view_x int8][view_y int8].
 * O hid_task lê via botoeira_get() e injeta nos botões/eixos HID.
 */

// LEGADO (não chamado): subia a UART2 dedicada (GPIO15/16). Mantida no .c só
// de referência; o caminho ativo é botoeira_rx_feed() via CAN.
void botoeira_rx_init(void);

// Último estado recebido (0 se nada chegou ainda).
void botoeira_get(uint16_t *buttons, int8_t *view_x, int8_t *view_y);

// Alimenta o estado a partir de um payload 0x040 já desembrulhado (4 bytes:
// [botoes u16][view_x i8][view_y i8]). Usado quando a botoeira chega pelo CAN
// (via on_module_msg) em vez da UART2.
void botoeira_rx_feed(const uint8_t *payload, uint8_t len);
