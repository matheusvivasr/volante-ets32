#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
 * lights.h – LEDs indicadores de farol no painel C3.
 *
 * Usam os pads rotulados RX/TX da placa = UART0 (GPIO20/21). Esses pinos ficaram
 * livres porque o console foi movido p/ USB-Serial-JTAG (sdkconfig); a UART de
 * comunicacao com a S3 e a UART1 (GPIO0/1), entao nada de comms passa por aqui.
 *   farol BAIXO -> GPIO20 (pad "RX")
 *   farol ALTO  -> GPIO21 (pad "TX")
 *
 * Acendem conforme a telemetria 0x101 que a C3 ja recebe (LED_FAROL_BAIXO bit 3,
 * LED_FAROL_ALTO bit 4). Saida ACTIVE-HIGH para LED comum (GPIO alto = aceso, com
 * o LED em serie com um resistor ao GND). Para um modulo de rele active-low,
 * basta trocar LIGHT_ACTIVE_HIGH para false.
 *
 * OBS: no boot, o ROM bootloader ainda solta bytes na UART0 (GPIO21) por um
 * instante antes do lights_init() assumir o pino — um leve piscar no LED do
 * farol alto na energizacao, inofensivo.
 */

#define LIGHT_PIN_BAIXO     20     // pad "RX"
#define LIGHT_PIN_ALTO      21     // pad "TX"
#define LIGHT_ACTIVE_HIGH   true   // true = LED comum (GPIO alto acende)

void lights_init(void);

// Espelha farol baixo/alto nos pinos a partir do bitmap 0x101. Com fresh=false
// (telemetria velha / sem sinal) apaga os dois.
void lights_update(uint16_t led_flags, bool fresh);
