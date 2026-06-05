#pragma once
#include <stdbool.h>

/*
 * relay.h – dois reles que reproduzem o "tic-tac" do pisca de um carro,
 * um para CADA lado da seta (estalos independentes esq/dir, igual carro real).
 *
 * Acompanham a MESMA fase de blink dos telltales na tela (ver display.c
 * draw_indicators(): esp_timer_get_time()/450000, ~1.1 Hz): cada rele energiza
 * quando a seta do seu lado ACENDE (tic) e solta quando APAGA (tac). No
 * pisca-alerta os dois lados acionam juntos.
 *
 * Hardware (jun/2026): modulo de 2 reles ACTIVE-LOW, rele SRD-05VDC (bobina 5V)
 * alimentada pelo pino 5V da PROPRIA C3 no JD-VCC; lado logico (VCC) em 3.3V;
 * jumper VCC/JD-VCC REMOVIDO; GND comum com a C3.
 *   - Seta ESQUERDA  -> IN1 -> GPIO2
 *   - Seta DIREITA   -> IN2 -> GPIO8
 * GPIO2 e GPIO8 sao strapping (precisam ficar ALTO no boot); como o modulo
 * active-low fica em repouso ALTO, o boot continua seguro.
 */

#define RELAY_PIN_L        2      // seta ESQUERDA -> IN1
#define RELAY_PIN_R        8      // seta DIREITA  -> IN2
#define RELAY_ACTIVE_HIGH  false  // false = modulo active-low (aciona em nivel BAIXO)

void relay_init(void);

// Liga/desliga o "modo pisca" de cada lado. Para cada lado: quando true E a fase
// de blink esta acesa, o rele daquele lado clica. Chamar do loop de render com
// (telemetria fresca && seta/alerta daquele lado).
void relay_set_sides(bool left, bool right);
