#include "leds.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static uint16_t s_flags = 0;
static uint8_t  s_blink_phase = 0; // alterna a cada tick para piscar

static void gpio_out(int pin, int level)
{
    gpio_set_level(pin, level);
}

void leds_init(void)
{
    int pins[] = {
        LED_PIN_SETA_ESQ, LED_PIN_SETA_DIR,
        LED_PIN_FAROL, LED_PIN_FREIO_MAO, LED_PIN_LOW_FUEL
    };
    gpio_config_t cfg = {
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = 0,
        .pull_down_en = 0,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    for (int i = 0; i < 5; i++) {
        cfg.pin_bit_mask = (1ULL << pins[i]);
        gpio_config(&cfg);
        gpio_set_level(pins[i], 0);
    }
}

void leds_update(uint16_t flags)
{
    s_flags = flags;
}

void leds_tick(void)
{
    s_blink_phase ^= 1;

    // Seta esquerda pisca a 500ms (blink_phase muda a cada 250ms se tick a 4Hz)
    int seta_esq = (s_flags & (LED_SETA_ESQ | LED_PISCA_ALERT)) && s_blink_phase;
    int seta_dir = (s_flags & (LED_SETA_DIR | LED_PISCA_ALERT)) && s_blink_phase;

    gpio_out(LED_PIN_SETA_ESQ, seta_esq);
    gpio_out(LED_PIN_SETA_DIR, seta_dir);

    // Farol: alto pisca rápido, baixo fixo
    int farol = (s_flags & LED_FAROL_ALTO)
                ? s_blink_phase                    // alto: pisca
                : !!(s_flags & LED_FAROL_BAIXO);   // baixo: fixo
    gpio_out(LED_PIN_FAROL, farol);

    gpio_out(LED_PIN_FREIO_MAO, !!(s_flags & LED_FREIO_MAO));
    gpio_out(LED_PIN_LOW_FUEL,  !!(s_flags & LED_LOW_FUEL) && s_blink_phase);
}
