#include "inputs.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

// Ordem dos bits = ordem deste vetor (bit0 = primeiro).
static const int s_btn_gpio[8] = { 5, 6, 7, 10, 20, 21, 2, 8 };

static adc_oneshot_unit_handle_t s_adc;

#define JOY_X_CH  ADC_CHANNEL_3   // GPIO3
#define JOY_Y_CH  ADC_CHANNEL_4   // GPIO4

void inputs_init(void)
{
    // Botões: entrada com pull-up (ativo em 0)
    uint64_t mask = 0;
    for (int i = 0; i < 8; i++) mask |= (1ULL << s_btn_gpio[i]);
    gpio_config_t io = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io);

    // Joystick: ADC1 oneshot, atenuação 12dB (~0..3.1V)
    adc_oneshot_unit_init_cfg_t uc = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&uc, &s_adc));
    adc_oneshot_chan_cfg_t cc = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, JOY_X_CH, &cc));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, JOY_Y_CH, &cc));

    ESP_LOGI("inputs", "8 botoes + joystick (VRx=GPIO3 VRy=GPIO4) prontos");
}

static int8_t adc_to_axis(int raw)
{
    int v = (raw - 2048) * 127 / 2048;   // centro 2048 -> 0
    if (v > 127)  v = 127;
    if (v < -127) v = -127;
    return (int8_t)v;
}

void inputs_read(inputs_state_t *st)
{
    uint16_t b = 0;
    for (int i = 0; i < 8; i++)
        if (gpio_get_level(s_btn_gpio[i]) == 0) b |= (1 << i);   // ativo-baixo
    st->buttons = b;

    int rx = 0, ry = 0;
    adc_oneshot_read(s_adc, JOY_X_CH, &rx);
    adc_oneshot_read(s_adc, JOY_Y_CH, &ry);
    st->view_x = adc_to_axis(rx);
    st->view_y = adc_to_axis(ry);
}
