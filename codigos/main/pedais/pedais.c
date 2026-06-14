/*
 * pedais.c – acelerador/freio. Dois caminhos de codigo selecionados por
 * PEDAIS_USE_ADC (ver pedais.h): botoes digitais (agora) ou ADC (futuro).
 */

#include "pedais.h"
#include "esp_log.h"

static const char *TAG = "pedais";

#define PIN_ACCEL  10   // GPIO10 = ADC1_CH9
#define PIN_BRAKE   9   // GPIO9  = ADC1_CH8

#if !PEDAIS_USE_ADC
// ─────────────────────────── MODO BOTAO (digital) ───────────────────────────
#include "driver/gpio.h"

void pedais_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_ACCEL) | (1ULL << PIN_BRAKE),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // ativo-baixo: GPIO -> botao -> GND
    };
    gpio_config(&io);
    ESP_LOGI(TAG, "modo BOTAO: accel=GPIO%d freio=GPIO%d (ativo-baixo)", PIN_ACCEL, PIN_BRAKE);
}

void pedais_get(int8_t *accel, int8_t *brake)
{
    // Pressionado (nivel 0) = fundo (+127); solto = repouso (-127).
    *accel = (gpio_get_level(PIN_ACCEL) == 0) ? 127 : -127;
    *brake = (gpio_get_level(PIN_BRAKE) == 0) ? 127 : -127;
}

#else
// ─────────────────────────── MODO POTENCIOMETRO (ADC) ───────────────────────
#include "esp_adc/adc_oneshot.h"

#define ADC_CH_ACCEL  ADC_CHANNEL_9   // GPIO10
#define ADC_CH_BRAKE  ADC_CHANNEL_8   // GPIO9

// Calibracao bruta dos cursores (0..4095). Ajuste apos montar as molas:
// curso util do potenciometro de cada pedal.
#define ACCEL_RAW_MIN   200
#define ACCEL_RAW_MAX  3900
#define BRAKE_RAW_MIN   200
#define BRAKE_RAW_MAX  3900

static adc_oneshot_unit_handle_t s_adc;

static int8_t map_raw(int raw, int lo, int hi)
{
    if (raw < lo) raw = lo;
    if (raw > hi) raw = hi;
    // lo -> -127 (repouso) ; hi -> +127 (fundo)
    return (int8_t)((int32_t)(raw - lo) * 254 / (hi - lo) - 127);
}

void pedais_init(void)
{
    adc_oneshot_unit_init_cfg_t unit = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit, &s_adc));

    adc_oneshot_chan_cfg_t ch = {
        .atten    = ADC_ATTEN_DB_12,   // faixa cheia ~0..3.3V
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, ADC_CH_ACCEL, &ch));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, ADC_CH_BRAKE, &ch));
    ESP_LOGI(TAG, "modo POTENCIOMETRO: accel=GPIO%d freio=GPIO%d (ADC1)", PIN_ACCEL, PIN_BRAKE);
}

void pedais_get(int8_t *accel, int8_t *brake)
{
    int ra = 0, rb = 0;
    adc_oneshot_read(s_adc, ADC_CH_ACCEL, &ra);
    adc_oneshot_read(s_adc, ADC_CH_BRAKE, &rb);
    *accel = map_raw(ra, ACCEL_RAW_MIN, ACCEL_RAW_MAX);
    *brake = map_raw(rb, BRAKE_RAW_MIN, BRAKE_RAW_MAX);
}

#endif
