#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
 * usb_hid.h – S3 como gamepad USB (HID) pela USB NATIVA (GPIO19/20).
 *
 * Etapa 2: faz o ETS2 (e o Windows) enxergarem a S3 como um controle, com um
 * eixo de direcao (X). Por ora o eixo e int8 (-127..127); resolucao maior (16
 * bits p/ os 14 bits do AS5047P) fica p/ depois, com um report descriptor custom.
 *
 * IMPORTANTE: a USB nativa e uma porta SEPARADA do CH343 (COM3, gravacao/console).
 * Para o controle aparecer no PC, plugue o cabo na porta USB NATIVA da S3.
 */

void usb_hid_init(void);

// true quando o host montou o dispositivo (cabo na USB nativa + enumerado).
bool usb_hid_ready(void);

// Envia um relatorio de gamepad. Mapa de eixos:
//   X  = direcao (volante, AS5047P calibrado)
//   Y  = acelerador (pedais)
//   Rx = freio (pedais)
//   Z  = visao horizontal (joystick da Botoeira)
//   Rz = visao vertical   (joystick da Botoeira)
// buttons = bitmap (bits 0-7 = botoes da Botoeira; bit 15 = BOOT local de teste).
void usb_hid_send(int8_t steering_x, int8_t throttle_y, int8_t brake_rx,
                  int8_t view_x, int8_t view_y, uint32_t buttons);
