/*
 * relay.c – aciona dois reles (esq/dir) em fase com o pisca dos telltales.
 *
 * Uma task le a mesma fase de blink usada no display (display.c) e, para cada
 * lado cujo "modo pisca" estiver ligado, energiza/solta o rele a cada meio-ciclo.
 * Cada transicao e um clique mecanico, reproduzindo o som do pisca de um carro
 * com estalos independentes para cada lado.
 */

#include "relay.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Mesma cadencia do blink dos telltales no display.c (450 ms => ~1.1 Hz).
// Manter igual ao de draw_indicators() para o clique casar com a tela.
#define BLINK_HALF_US 450000

static volatile bool s_left  = false;   // lado esquerdo ativo (seta esq / alerta)
static volatile bool s_right = false;   // lado direito ativo (seta dir / alerta)
static bool s_on_l = false;             // estado fisico atual do rele esquerdo
static bool s_on_r = false;             // estado fisico atual do rele direito

static void relay_write(int pin, bool *cur, bool on)
{
    *cur = on;
    // active-low: acionado => nivel BAIXO; repouso => nivel ALTO (strapping ok no boot).
    int level = (on == RELAY_ACTIVE_HIGH) ? 1 : 0;
    gpio_set_level(pin, level);
}

static void relay_task(void *arg)
{
    while (1) {
        int blink = (esp_timer_get_time() / BLINK_HALF_US) & 1;   // mesma fase do display
        bool wl = s_left  && blink;
        bool wr = s_right && blink;
        if (wl != s_on_l) relay_write(RELAY_PIN_L, &s_on_l, wl);   // cada transicao = 1 clique
        if (wr != s_on_r) relay_write(RELAY_PIN_R, &s_on_r, wr);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void relay_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << RELAY_PIN_L) | (1ULL << RELAY_PIN_R),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    relay_write(RELAY_PIN_L, &s_on_l, false);   // comeca solto (active-low => GPIO em ALTO)
    relay_write(RELAY_PIN_R, &s_on_r, false);
    xTaskCreate(relay_task, "relay", 2048, NULL, 5, NULL);
}

void relay_set_sides(bool left, bool right)
{
    s_left  = left;
    s_right = right;
}
