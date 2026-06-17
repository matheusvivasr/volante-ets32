#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "transport_rx.h"
#include "display.h"
#include "oled.h"
#include "relay.h"
#include "lights.h"
#include "leds.h"   // só para as máscaras LED_SETA_*/LED_PISCA_ALERT/LED_FAROL_*
#include <string.h>

static const char *TAG = "painel_c3";

// Botão BOOT da C3 (GPIO9, ativo em 0) → troca de tela.
#define BTN_PIN 9
static volatile uint32_t s_btn_clicks = 0;   // incrementa a cada clique curto (troca tela)

// Põe 1 para gravar o firmware de DIAGNÓSTICO do display (diag.c) em vez do painel.
// Volte para 0 quando o display estiver funcionando.
#define RUN_DIAG 0

// ─── Imagem de estado local ───────────────────────────────────────────────────
static panel_data_t s_data = {0};
static uint32_t s_last_update_ms = 0;

#define TIMEOUT_MS    500    // sem mensagem por 500ms → sem sinal
#define TICK_MS       10     // render o mais rápido possível (limitado pelo flush SPI ~92ms)
#define VIEW_HOLD_MS  2000   // (não usado: navegação é só pelo botão)

// Anti-ghosting: faixa em NEGATIVO varrendo a tela, embutida no lcd_flush()
// (display.c) -> roda em todo flush, independente do conteúdo. Ajuste AG_BAND_*
// em display.c se quiser mais/menos agressivo.

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

// ─── Callback de mensagens recebidas da S3 ────────────────────────────────────
static void on_msg(const transport_msg_t *msg)
{
    const uint8_t *p = msg->payload;

    switch (msg->id) {
        case MSG_TELEM_PAINEL:
            if (msg->len >= 7) {
                uint16_t speed_x10 = p[0] | ((uint16_t)p[1] << 8);
                s_data.speed_kmh = speed_x10 / 10.0f;
                s_data.rpm       = p[2] | ((uint16_t)p[3] << 8);
                s_data.gear      = (int8_t)p[4];
                s_data.fuel_pct  = p[5];
                s_data.temp_c    = p[6];
            }
            break;

        case MSG_TELEM_LEDS:
            if (msg->len >= 2)
                s_data.led_flags = p[0] | ((uint16_t)p[1] << 8);
            break;

        case MSG_TELEM_NAV:
            if (msg->len >= 7) {
                s_data.dist_m  = p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
                s_data.eta_min = p[3] | ((uint16_t)p[4] << 8);
                s_data.odo_km  = p[5] | ((uint16_t)p[6] << 8);
            }
            break;

        default:
            break;
    }

    s_last_update_ms = now_ms();
}

// ─── Task do botão: detecta clique (borda de descida) a 25ms ─────────────────
static void button_task(void *arg)
{
    int prev = 1;
    while (1) {
        int b = gpio_get_level(BTN_PIN);
        if (prev == 1 && b == 0) s_btn_clicks++;   // pressionou (clique curto = troca tela)
        prev = b;
        vTaskDelay(pdMS_TO_TICKS(25));
    }
}

// ─── Task de renderização (alterna views a cada 2s ou no clique do botão) ─────
static void render_task(void *arg)
{
    panel_view_t view = VIEW_VELOCIDADE;
    uint32_t clicks_seen = s_btn_clicks;
    int last_fuel = -2;

    // Detecção de tela estática p/ o anti-ghost automático.
    panel_data_t prev_data = {0};
    int  prev_view  = -1;
    bool prev_fresh = false;
    uint32_t last_change = now_ms();

    while (1) {
        uint32_t t = now_ms();

        // Navegação é SÓ pelo botão (sem giro automático).
        if (s_btn_clicks != clicks_seen) {
            clicks_seen = s_btn_clicks;
            view = (view + 1) % VIEW_COUNT;
        }

        // Relés do pisca: um por lado da seta, clicando em fase com os telltales
        // (só com telemetria fresca, p/ não ficar clicando sozinho sem sinal).
        // Pisca-alerta (LED_PISCA_ALERT) aciona os dois lados juntos.
        bool fresh = (t - s_last_update_ms) <= TIMEOUT_MS;
        relay_set_sides(
            fresh && (s_data.led_flags & (LED_SETA_ESQ | LED_PISCA_ALERT)),
            fresh && (s_data.led_flags & (LED_SETA_DIR | LED_PISCA_ALERT)));

        // LEDs de farol baixo/alto nos pads RX/TX (GPIO20/21).
        lights_update(s_data.led_flags, fresh);

        if (t - s_last_update_ms > TIMEOUT_MS) {
            display_no_signal();
            if (last_fuel != -1) { oled_fuel(-1); last_fuel = -1; }   // bateria vazia
        } else {
            display_show(&s_data, view);
            if ((int)s_data.fuel_pct != last_fuel) {   // OLED só quando muda
                oled_fuel(s_data.fuel_pct);
                last_fuel = (int)s_data.fuel_pct;
            }
        }

        // ── Anti-ghost AUTOMÁTICO: se a TELA ficar estática (conteúdo idêntico) por
        // mais de 3 min, vai pro modo agressivo (8 barras); senão, faixa fina (modo 1).
        // Jogando, a telemetria muda toda hora -> nunca passa de 3 min parado.
        bool changed = (view != prev_view) || (fresh != prev_fresh)
                       || memcmp(&s_data, &prev_data, sizeof(panel_data_t)) != 0;
        if (changed) {
            prev_data   = s_data;
            prev_view   = view;
            prev_fresh  = fresh;
            last_change = t;
        }
        display_set_ag_mode((t - last_change >= 3u * 60u * 1000u) ? 2 : 1);

        vTaskDelay(pdMS_TO_TICKS(TICK_MS));
    }
}

// ─── app_main ─────────────────────────────────────────────────────────────────
void app_main(void)
{
#if RUN_DIAG
    extern void diag_run(void);
    diag_run();   // não retorna
    return;
#endif

    ESP_LOGI(TAG, "=== Volante DIY – Painel C3 (GC9A01) ===");
    ESP_LOGI(TAG, "Etapa 1: recebe telemetria da S3 e exibe no TFT redondo");

    display_init();
    oled_init();
    relay_init();
    lights_init();
    transport_rx_init(on_msg);

    // Botão BOOT (GPIO9) como troca de tela: entrada com pull-up (ativo em 0).
    gpio_config_t btn = {
        .pin_bit_mask = (1ULL << BTN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn);

    // Tela de boot rápida (barras de cor) ~2.5s.
    display_boot();
    oled_fuel(-1);
    vTaskDelay(pdMS_TO_TICKS(2500));

    display_no_signal();
    vTaskDelay(pdMS_TO_TICKS(300));

    xTaskCreate(button_task, "button", 2048, NULL, 5, NULL);
    xTaskCreate(render_task, "render", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "Pronto. Aguardando telemetria da S3...");
}
