#pragma once
#include <stdint.h>

/*
 * pedais.h – acelerador e freio na propria S3 (eixos HID).
 *
 * Hoje sao 2 BOTOES (digitais). No futuro viram 2 POTENCIOMETROS em molas
 * (pedais analogicos de verdade). A escolha e por macro de compilacao:
 *
 *     PEDAIS_USE_ADC = 0  -> 2 botoes  (GPIO ativo-baixo, pull-up interno)
 *     PEDAIS_USE_ADC = 1  -> 2 potenciometros (ADC1)
 *
 * Pinos (os MESMOS pontos de solda servem nos dois modos — sao ADC-capazes):
 *     Acelerador = GPIO10 (ADC1_CH9)
 *     Freio      = GPIO9  (ADC1_CH8)
 *
 * Botao: aterra o GPIO (GPIO -> botao -> GND). Potenciometro: extremos em
 * 3V3/GND e o cursor no GPIO (NUNCA 5V no cursor — estoura o ADC).
 *
 * Saida: -127 (solto/repouso) .. +127 (fundo). O ETS2 calibra o resto.
 */

#define PEDAIS_USE_ADC 0   // 0 = botoes (agora) | 1 = potenciometros (futuro)

void pedais_init(void);

// Devolve acelerador e freio ja mapeados em -127..127.
void pedais_get(int8_t *accel, int8_t *brake);
