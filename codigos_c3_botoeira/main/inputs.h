#pragma once
#include <stdint.h>

/*
 * inputs.h – leitura dos 8 botões + joystick analógico (visão) da Botoeira.
 *
 * Botões (ativo-baixo, pull-up interno) -> bits do bitmap:
 *   bit0 seta esq (GPIO5)   bit4 farol alto (GPIO20)
 *   bit1 seta dir (GPIO6)   bit5 farol baixo (GPIO21)
 *   bit2 alerta   (GPIO7)   bit6 limpador (GPIO2)  *strapping
 *   bit3 buzina   (GPIO10)  bit7 freio-motor (GPIO8) *strapping
 *
 * Joystick: VRx=GPIO3 (ADC1_CH3), VRy=GPIO4 (ADC1_CH4), VCC=3.3V.
 */

typedef struct {
    uint16_t buttons;   // bitmap (bit0..bit7)
    int8_t   view_x;    // joystick horizontal (-127..127, centro 0)
    int8_t   view_y;    // joystick vertical
} inputs_state_t;

void inputs_init(void);
void inputs_read(inputs_state_t *st);
