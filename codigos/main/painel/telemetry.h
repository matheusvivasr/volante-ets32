#pragma once

#include <stdint.h>
#include <stdbool.h>

// ─── Estruturas de estado da telemetria em RAM (imagem local) ─────────────────

// Grupo 1 – Painel (MSG 0x100)
typedef struct {
    uint16_t speed_kmh_x10;   // km/h * 10  (ex: 875 = 87.5 km/h)
    uint16_t rpm;
    int8_t   gear;            // -1=ré, 0=neutro, 1..n
    uint8_t  fuel_pct;        // 0-100
    uint8_t  engine_temp_c;
    uint8_t  seq;
    bool     valid;           // false se timeout
} telem_painel_t;

// Grupo 2 – Indicadores luminosos (MSG 0x101)
typedef struct {
    uint16_t flags;           // bitmap – veja defines abaixo
    uint8_t  seq;
    bool     valid;
} telem_leds_t;

#define LED_SETA_ESQ    (1 << 0)
#define LED_SETA_DIR    (1 << 1)
#define LED_PISCA_ALERT (1 << 2)
#define LED_FAROL_BAIXO (1 << 3)
#define LED_FAROL_ALTO  (1 << 4)
#define LED_LUZ_FREIO   (1 << 5)
#define LED_FREIO_MAO   (1 << 6)
#define LED_FREIO_MOTOR (1 << 7)
#define LED_CRUISE_ON   (1 << 8)
#define LED_BUZINA      (1 << 9)
#define LED_LIMPADOR    (1 << 10)
#define LED_LOW_FUEL    (1 << 11)

// Grupo 3 – Navegação (MSG 0x102)
typedef struct {
    uint32_t dist_dest_m;     // distância até destino em metros
    uint16_t eta_min;         // tempo estimado em minutos
    uint16_t odometer_km;
    uint8_t  seq;
    bool     valid;
} telem_nav_t;

// ─── API ──────────────────────────────────────────────────────────────────────
void telemetry_init(void);

// Atualiza a imagem local a partir de um payload cru recebido do PC
void telemetry_update_painel(const uint8_t *payload, uint8_t len, uint8_t seq);
void telemetry_update_leds(const uint8_t *payload, uint8_t len, uint8_t seq);
void telemetry_update_nav(const uint8_t *payload, uint8_t len, uint8_t seq);

// Lê a imagem atual (thread-safe via mutex interno)
void telemetry_get_painel(telem_painel_t *out);
void telemetry_get_leds(telem_leds_t *out);
void telemetry_get_nav(telem_nav_t *out);

// Chama periodicamente para detectar timeout (200ms sem update → valid=false)
void telemetry_tick(void);

// Serializa e envia os 3 grupos pelo transporte (para o painel C3)
void telemetry_reemit(void);
