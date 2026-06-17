/*
 * calib.c – calibracao do eixo de direcao + maquina de estados do botao BOOT.
 * Ver calib.h para a visao geral e o roteiro de calibracao.
 */

#include "calib.h"
#include "as5600.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "calib";

#define BOOT_BTN_PIN     0            // botao BOOT da S3 (ativo-baixo)
#define BTN_DEBOUNCE_MS  40
#define BTN_LONG_MS      1500         // segurar p/ entrar/cancelar calibracao

#define NVS_NS    "volante"
#define NVS_KEY   "steer_cal"
#define CAL_MAGIC 0x5A47C001

// ── Calibracao persistida (frame do angulo continuo) ──
typedef struct {
    uint32_t magic;
    int32_t  left;     // continuo no batente esquerdo
    int32_t  right;    // continuo no batente direito
    int32_t  center;   // continuo no centro
} steer_cal_t;

// Default: 900 graus lock-to-lock (~2.5 voltas), centro em 0. Substituido pela
// calibracao real assim que o usuario rodar o roteiro do BOOT.
#define DEG2CONT(d) ((int32_t)((d) / 360.0f * AS5600_CPR))
static steer_cal_t s_cal = {
    .magic  = CAL_MAGIC,
    .left   = -DEG2CONT(450),
    .right  =  DEG2CONT(450),
    .center =  0,
};

// ── Estado do multi-turn ──
static int32_t s_prev_raw  = -1;
static int64_t s_turns     = 0;
static int32_t s_origin    = 0;        // raw lido no boot (= centro presumido)
static bool    s_have_orig = false;
static int32_t s_cont      = 0;        // ultimo angulo continuo valido

// ── Estado do botao BOOT ──
typedef enum { CAL_IDLE, CAL_LEFT, CAL_RIGHT, CAL_CENTER } cal_state_t;
static cal_state_t s_state = CAL_IDLE;

static bool    s_btn_down   = false;   // estado debounced
static int64_t s_edge_us    = 0;       // instante da ultima borda estavel
static bool    s_raw_prev   = false;   // leitura crua anterior (p/ debounce)
static int64_t s_raw_edge_us = 0;
static bool    s_press_consumed = false;

// ── Persistencia ──────────────────────────────────────────────────────────────
static void cal_load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGW(TAG, "sem NVS; usando calibracao padrao (900 deg). Calibre pelo BOOT.");
        return;
    }
    steer_cal_t tmp;
    size_t sz = sizeof(tmp);
    esp_err_t err = nvs_get_blob(h, NVS_KEY, &tmp, &sz);
    nvs_close(h);
    if (err == ESP_OK && sz == sizeof(tmp) && tmp.magic == CAL_MAGIC) {
        s_cal = tmp;
        ESP_LOGI(TAG, "calibracao carregada: esq=%d centro=%d dir=%d",
                 (int)s_cal.left, (int)s_cal.center, (int)s_cal.right);
    } else {
        ESP_LOGW(TAG, "NVS vazio/invalido; calibracao padrao. Calibre pelo BOOT.");
    }
}

static void cal_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open falhou ao salvar");
        return;
    }
    s_cal.magic = CAL_MAGIC;
    esp_err_t err = nvs_set_blob(h, NVS_KEY, &s_cal, sizeof(s_cal));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK)
        ESP_LOGI(TAG, "calibracao SALVA: esq=%d centro=%d dir=%d",
                 (int)s_cal.left, (int)s_cal.center, (int)s_cal.right);
    else
        ESP_LOGE(TAG, "falha ao salvar calibracao (%s)", esp_err_to_name(err));
}

// ── API ─────────────────────────────────────────────────────────────────────
void calib_init(void)
{
    gpio_config_t btn = {
        .pin_bit_mask = (1ULL << BOOT_BTN_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&btn);

    cal_load();
    ESP_LOGI(TAG, "BOOT: segure >1.5s p/ calibrar a direcao (esq -> dir -> centro).");
}

int8_t calib_steering(int32_t raw)
{
    if (raw < 0) return 0;                      // sem sensor -> centro

    if (!s_have_orig) {                          // primeira leitura = centro presumido
        s_origin = raw;
        s_prev_raw = raw;
        s_have_orig = true;
    }

    int32_t diff = raw - s_prev_raw;
    if (diff < -AS5600_CPR / 2) s_turns++;       // wrap 16383 -> 0 (sentido +)
    else if (diff > AS5600_CPR / 2) s_turns--;   // wrap 0 -> 16383 (sentido -)
    s_prev_raw = raw;

    s_cont = (int32_t)(s_turns * AS5600_CPR + (raw - s_origin));

    // Mapeia continuo -> -127..127 com dois lados independentes (esq/dir).
    double dc = (double)(s_cont - s_cal.center);
    double dl = (double)(s_cal.left  - s_cal.center);
    double dr = (double)(s_cal.right - s_cal.center);
    double x;
    if (dc == 0.0 || (dl == 0.0 && dr == 0.0)) {
        x = 0.0;
    } else if (dc * dl >= 0.0 && dl != 0.0) {    // lado esquerdo
        double f = dc / dl;                       // 0..1
        if (f > 1.0) f = 1.0;
        x = -127.0 * f;
    } else {                                      // lado direito
        double f = (dr != 0.0) ? dc / dr : 0.0;   // 0..1
        if (f > 1.0) f = 1.0;
        if (f < 0.0) f = 0.0;
        x = 127.0 * f;
    }
    if (x > 127.0) x = 127.0;
    if (x < -127.0) x = -127.0;
    return (int8_t)x;
}

bool calib_is_calibrating(void) { return s_state != CAL_IDLE; }

bool calib_hid_button(void) { return s_state == CAL_IDLE && s_btn_down; }

// Trata um clique CURTO conforme o estado da calibracao.
static void on_short_press(void)
{
    switch (s_state) {
        case CAL_IDLE:                            // curto em IDLE = botao 16 (HID), nada aqui
            break;
        case CAL_LEFT:
            s_cal.left = s_cont;
            ESP_LOGI(TAG, "[calib] batente ESQUERDO = %d. Gire p/ DIREITA e clique.", (int)s_cont);
            s_state = CAL_RIGHT;
            break;
        case CAL_RIGHT:
            s_cal.right = s_cont;
            ESP_LOGI(TAG, "[calib] batente DIREITO = %d. CENTRE o volante e clique.", (int)s_cont);
            s_state = CAL_CENTER;
            break;
        case CAL_CENTER:
            s_cal.center = s_cont;
            ESP_LOGI(TAG, "[calib] CENTRO = %d. Salvando...", (int)s_cont);
            cal_save();
            s_state = CAL_IDLE;
            break;
    }
}

// Trata um clique LONGO: entra (em IDLE) ou cancela (durante a calibracao).
static void on_long_press(void)
{
    if (s_state == CAL_IDLE) {
        s_state = CAL_LEFT;
        ESP_LOGI(TAG, "[calib] MODO CALIBRACAO. Gire TOTALMENTE p/ ESQUERDA e clique BOOT.");
    } else {
        s_state = CAL_IDLE;
        ESP_LOGW(TAG, "[calib] CANCELADO. Calibracao anterior mantida.");
    }
}

void calib_button_poll(void)
{
    int64_t now = esp_timer_get_time();
    bool raw_down = (gpio_get_level(BOOT_BTN_PIN) == 0);

    // Debounce: so aceita a leitura crua se ela ficou estavel por BTN_DEBOUNCE_MS.
    if (raw_down != s_raw_prev) {
        s_raw_prev = raw_down;
        s_raw_edge_us = now;
    }
    if ((now - s_raw_edge_us) < BTN_DEBOUNCE_MS * 1000) return;

    if (raw_down != s_btn_down) {                 // borda debounced
        s_btn_down = raw_down;
        if (raw_down) {                            // pressionou
            s_edge_us = now;
            s_press_consumed = false;
        } else {                                   // soltou
            int64_t held = now - s_edge_us;
            if (!s_press_consumed) {
                if (held >= BTN_LONG_MS * 1000) on_long_press();
                else                            on_short_press();
            }
        }
    } else if (s_btn_down && !s_press_consumed) {
        // Long-press dispara assim que cruza o limite, sem esperar soltar.
        if ((now - s_edge_us) >= BTN_LONG_MS * 1000) {
            on_long_press();
            s_press_consumed = true;               // ignora o "soltar" subsequente
        }
    }
}
