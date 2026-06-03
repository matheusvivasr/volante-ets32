#pragma once
#include <stdint.h>

// Mapeamento dos LEDs indicadores – ajuste os GPIOs para o seu hardware
// Se você tiver uma barra de LEDs WS2812 (NeoPixel), pode trocar por um
// único GPIO + driver RMT. Por ora, GPIOs individuais.

#define LED_PIN_SETA_ESQ    0   // GPIO0  – seta esquerda
#define LED_PIN_SETA_DIR    1   // GPIO1  – seta direita
#define LED_PIN_FAROL       2   // GPIO2  – farol baixo/alto
#define LED_PIN_FREIO_MAO   3   // GPIO3  – freio de mão
#define LED_PIN_LOW_FUEL    8   // GPIO8  – combustível baixo (built-in no C3 DevKit)

// Flags (mesmas do Python e da S3)
#define LED_SETA_ESQ    (1 << 0)
#define LED_SETA_DIR    (1 << 1)
#define LED_PISCA_ALERT (1 << 2)
#define LED_FAROL_BAIXO (1 << 3)
#define LED_FAROL_ALTO  (1 << 4)
#define LED_FREIO_MAO   (1 << 6)
#define LED_LOW_FUEL    (1 << 11)

void leds_init(void);

// Atualiza todos os LEDs a partir do bitmap de flags
void leds_update(uint16_t flags);

// Pisca tarefa de seta (chama internamente, precisa de tick periódico)
void leds_tick(void);
