/*
 * lights.c – acende os LEDs de farol baixo/alto conforme a telemetria.
 *
 * Simples espelho dos bits LED_FAROL_BAIXO/LED_FAROL_ALTO (msg 0x101) em dois
 * GPIOs de saida (pads RX/TX = GPIO20/21). Chamado pelo loop de render.
 */

#include "lights.h"
#include "leds.h"          // LED_FAROL_BAIXO / LED_FAROL_ALTO
#include "driver/gpio.h"

static void light_write(int pin, bool on)
{
    gpio_set_level(pin, (on == LIGHT_ACTIVE_HIGH) ? 1 : 0);
}

void lights_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << LIGHT_PIN_BAIXO) | (1ULL << LIGHT_PIN_ALTO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    light_write(LIGHT_PIN_BAIXO, false);
    light_write(LIGHT_PIN_ALTO,  false);
}

void lights_update(uint16_t led_flags, bool fresh)
{
    light_write(LIGHT_PIN_BAIXO, fresh && (led_flags & LED_FAROL_BAIXO));
    light_write(LIGHT_PIN_ALTO,  fresh && (led_flags & LED_FAROL_ALTO));
}
