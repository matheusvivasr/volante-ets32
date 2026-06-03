#include "telemetry.h"
#include "transport.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "telemetry";

#define TIMEOUT_MS 200

// ─── Estado em RAM ────────────────────────────────────────────────────────────
static telem_painel_t s_painel = {0};
static telem_leds_t   s_leds   = {0};
static telem_nav_t    s_nav    = {0};

// Timestamps da última atualização (ms)
static uint32_t s_ts_painel = 0;
static uint32_t s_ts_leds   = 0;
static uint32_t s_ts_nav    = 0;

static SemaphoreHandle_t s_mutex;

// ─── Helpers ──────────────────────────────────────────────────────────────────
static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

// ─── Init ─────────────────────────────────────────────────────────────────────
void telemetry_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "telemetry init OK");
}

// ─── Updates (chamados quando chega dado do PC via USB serial) ────────────────
void telemetry_update_painel(const uint8_t *p, uint8_t len, uint8_t seq)
{
    if (len < 7) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_painel.speed_kmh_x10 = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    s_painel.rpm           = (uint16_t)p[2] | ((uint16_t)p[3] << 8);
    s_painel.gear          = (int8_t)p[4];
    s_painel.fuel_pct      = p[5];
    s_painel.engine_temp_c = p[6];
    s_painel.seq           = seq;
    s_painel.valid         = true;
    s_ts_painel            = now_ms();
    xSemaphoreGive(s_mutex);
}

void telemetry_update_leds(const uint8_t *p, uint8_t len, uint8_t seq)
{
    if (len < 2) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_leds.flags = (uint16_t)p[0] | ((uint16_t)p[1] << 8);
    s_leds.seq   = seq;
    s_leds.valid = true;
    s_ts_leds    = now_ms();
    xSemaphoreGive(s_mutex);
}

void telemetry_update_nav(const uint8_t *p, uint8_t len, uint8_t seq)
{
    if (len < 7) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_nav.dist_dest_m  = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    s_nav.eta_min      = (uint16_t)p[3] | ((uint16_t)p[4] << 8);
    s_nav.odometer_km  = (uint16_t)p[5] | ((uint16_t)p[6] << 8);
    s_nav.seq          = seq;
    s_nav.valid        = true;
    s_ts_nav           = now_ms();
    xSemaphoreGive(s_mutex);
}

// ─── Getters thread-safe ──────────────────────────────────────────────────────
void telemetry_get_painel(telem_painel_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_painel;
    xSemaphoreGive(s_mutex);
}

void telemetry_get_leds(telem_leds_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_leds;
    xSemaphoreGive(s_mutex);
}

void telemetry_get_nav(telem_nav_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_nav;
    xSemaphoreGive(s_mutex);
}

// ─── Timeout ──────────────────────────────────────────────────────────────────
void telemetry_tick(void)
{
    uint32_t t = now_ms();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_painel.valid && (t - s_ts_painel) > TIMEOUT_MS) {
        s_painel.valid = false;
        ESP_LOGW(TAG, "painel timeout");
    }
    if (s_leds.valid && (t - s_ts_leds) > TIMEOUT_MS) {
        s_leds.valid = false;
    }
    if (s_nav.valid && (t - s_ts_nav) > TIMEOUT_MS) {
        s_nav.valid = false;
    }
    xSemaphoreGive(s_mutex);
}

// ─── Reemissão para o painel C3 via UART ─────────────────────────────────────
void telemetry_reemit(void)
{
    transport_msg_t msg = {0};

    // 0x100 – painel
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    telem_painel_t p = s_painel;
    telem_leds_t   l = s_leds;
    telem_nav_t    n = s_nav;
    xSemaphoreGive(s_mutex);

    msg.id = MSG_ID_TELEM_PAINEL;
    msg.len = 7;
    msg.payload[0] = p.speed_kmh_x10 & 0xFF;
    msg.payload[1] = (p.speed_kmh_x10 >> 8) & 0xFF;
    msg.payload[2] = p.rpm & 0xFF;
    msg.payload[3] = (p.rpm >> 8) & 0xFF;
    msg.payload[4] = (uint8_t)p.gear;
    msg.payload[5] = p.fuel_pct;
    msg.payload[6] = p.engine_temp_c;
    msg.seq        = p.seq;
    transport_send(&msg);

    // 0x101 – LEDs
    msg.id = MSG_ID_TELEM_LEDS;
    msg.len = 2;
    msg.payload[0] = l.flags & 0xFF;
    msg.payload[1] = (l.flags >> 8) & 0xFF;
    msg.seq        = l.seq;
    transport_send(&msg);

    // 0x102 – navegação
    msg.id = MSG_ID_TELEM_NAV;
    msg.len = 7;
    msg.payload[0] = n.dist_dest_m & 0xFF;
    msg.payload[1] = (n.dist_dest_m >> 8) & 0xFF;
    msg.payload[2] = (n.dist_dest_m >> 16) & 0xFF;
    msg.payload[3] = n.eta_min & 0xFF;
    msg.payload[4] = (n.eta_min >> 8) & 0xFF;
    msg.payload[5] = n.odometer_km & 0xFF;
    msg.payload[6] = (n.odometer_km >> 8) & 0xFF;
    msg.seq        = n.seq;
    transport_send(&msg);
}
