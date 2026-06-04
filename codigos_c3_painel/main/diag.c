/*
 * diag.c – diagnóstico do display GC9A01 (sem mexer nos GPIOs).
 *
 * FASE A (fiação): pulsa cada pino de controle 0<->3.3V a 1Hz, um de cada vez.
 *   Probe o pino correspondente do DISPLAY com multímetro (DC V) e veja se oscila.
 *
 * FASE B (SPI/controlador): inicializa SPI a 10 MHz e fica alternando:
 *   GC9A01 (INVON) -> GC9A01 (INVOFF) -> ST7789, enchendo de RED/GREEN/BLUE/WHITE.
 *   Se qualquer combinação mostrar cor sólida, achamos o controlador/config certo.
 *
 * Tudo é logado por serial (USB-JTAG do C3) pra correlacionar tela x config.
 * Ativado por RUN_DIAG em main.c.
 */

#include "display.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "diag";

#define COL_RED   0xF800
#define COL_GREEN 0x07E0
#define COL_BLUE  0x001F
#define COL_WHITE 0xFFFF

static spi_device_handle_t s_spi;

// ─── SPI low-level (DC manual) ────────────────────────────────────────────────
static void lcd_cmd(uint8_t c)
{
    gpio_set_level(GC9A01_PIN_DC, 0);
    spi_transaction_t t = { .length = 8, .tx_buffer = &c };
    spi_device_polling_transmit(s_spi, &t);
}
static void lcd_dat(const uint8_t *d, int n)
{
    if (n <= 0) return;
    gpio_set_level(GC9A01_PIN_DC, 1);
    spi_transaction_t t = { .length = (size_t)n * 8, .tx_buffer = d };
    spi_device_polling_transmit(s_spi, &t);
}
static void lcd_d1(uint8_t b) { lcd_dat(&b, 1); }

static void set_win(int x0, int y0, int x1, int y1)
{
    uint8_t d[4];
    lcd_cmd(0x2A); d[0]=x0>>8; d[1]=x0; d[2]=x1>>8; d[3]=x1; lcd_dat(d,4);
    lcd_cmd(0x2B); d[0]=y0>>8; d[1]=y0; d[2]=y1>>8; d[3]=y1; lcd_dat(d,4);
    lcd_cmd(0x2C);
}

static void fill(uint16_t color)
{
    static uint8_t line[GC9A01_W * 2];
    uint8_t hi = color >> 8, lo = color & 0xFF;   // big-endian no fio
    for (int i = 0; i < GC9A01_W; i++) { line[2*i] = hi; line[2*i+1] = lo; }
    set_win(0, 0, GC9A01_W - 1, GC9A01_H - 1);
    gpio_set_level(GC9A01_PIN_DC, 1);
    for (int y = 0; y < GC9A01_H; y++) {
        spi_transaction_t t = { .length = (size_t)GC9A01_W * 2 * 8, .tx_buffer = line };
        spi_device_polling_transmit(s_spi, &t);
    }
}

static void show_colors(void)
{
    struct { const char *n; uint16_t c; } cs[] = {
        {"RED", COL_RED}, {"GREEN", COL_GREEN}, {"BLUE", COL_BLUE}, {"WHITE", COL_WHITE},
    };
    for (int i = 0; i < 4; i++) {
        ESP_LOGW(TAG, "     tela deveria estar %s", cs[i].n);
        fill(cs[i].c);
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}

// ─── Inits ────────────────────────────────────────────────────────────────────
typedef struct { uint8_t cmd; uint8_t len; uint8_t data[16]; uint16_t delay_ms; } cmd_t;

static void run_seq(const cmd_t *seq)
{
    for (const cmd_t *c = seq; c->cmd != 0xFF || c->len != 0xFF; c++) {
        lcd_cmd(c->cmd);
        for (int i = 0; i < c->len; i++) lcd_d1(c->data[i]);
        if (c->delay_ms) vTaskDelay(pdMS_TO_TICKS(c->delay_ms));
    }
}

static const cmd_t GC9A01_SEQ[] = {
    {0x01,0,{0},120}, // software reset
    {0xEF,0,{0},0},{0xEB,1,{0x14},0},{0xFE,0,{0},0},{0xEF,0,{0},0},{0xEB,1,{0x14},0},
    {0x84,1,{0x40},0},{0x85,1,{0xFF},0},{0x86,1,{0xFF},0},{0x87,1,{0xFF},0},
    {0x88,1,{0x0A},0},{0x89,1,{0x21},0},{0x8A,1,{0x00},0},{0x8B,1,{0x80},0},
    {0x8C,1,{0x01},0},{0x8D,1,{0x01},0},{0x8E,1,{0xFF},0},{0x8F,1,{0xFF},0},
    {0xB6,2,{0x00,0x20},0},{0x36,1,{0x48},0},{0x3A,1,{0x05},0},
    {0x90,4,{0x08,0x08,0x08,0x08},0},{0xBD,1,{0x06},0},{0xBC,1,{0x00},0},
    {0xFF,3,{0x60,0x01,0x04},0},{0xC3,1,{0x13},0},{0xC4,1,{0x13},0},{0xC9,1,{0x22},0},
    {0xBE,1,{0x11},0},{0xE1,2,{0x10,0x0E},0},{0xDF,3,{0x21,0x0C,0x02},0},
    {0xF0,6,{0x45,0x09,0x08,0x08,0x26,0x2A},0},{0xF1,6,{0x43,0x70,0x72,0x36,0x37,0x6F},0},
    {0xF2,6,{0x45,0x09,0x08,0x08,0x26,0x2A},0},{0xF3,6,{0x43,0x70,0x72,0x36,0x37,0x6F},0},
    {0xED,2,{0x1B,0x0B},0},{0xAE,1,{0x77},0},{0xCD,1,{0x63},0},
    {0x70,9,{0x07,0x07,0x04,0x0E,0x0F,0x09,0x07,0x08,0x03},0},{0xE8,1,{0x34},0},
    {0x62,12,{0x18,0x0D,0x71,0xED,0x70,0x70,0x18,0x0F,0x71,0xEF,0x70,0x70},0},
    {0x63,12,{0x18,0x11,0x71,0xF1,0x70,0x70,0x18,0x13,0x71,0xF3,0x70,0x70},0},
    {0x64,7,{0x28,0x29,0xF1,0x01,0xF1,0x00,0x07},0},
    {0x66,10,{0x3C,0x00,0xCD,0x67,0x45,0x45,0x10,0x00,0x00,0x00},0},
    {0x67,10,{0x00,0x3C,0x00,0x00,0x00,0x01,0x54,0x10,0x32,0x98},0},
    {0x74,7,{0x10,0x85,0x80,0x00,0x00,0x4E,0x00},0},{0x98,2,{0x3E,0x07},0},
    {0x35,0,{0},0},{0x21,0,{0},0},{0x11,0,{0},120},{0x29,0,{0},20},
    {0xFF,0xFF,{0},0},
};

static const cmd_t ST7789_SEQ[] = {
    {0x01,0,{0},150}, // software reset
    {0x11,0,{0},120}, // sleep out
    {0x3A,1,{0x55},0},// 16-bit
    {0x36,1,{0x00},0},// MADCTL
    {0x21,0,{0},0},   // inversion on
    {0x13,0,{0},10},  // normal display
    {0x29,0,{0},50},  // display on
    {0xFF,0xFF,{0},0},
};

// ─── Fase A: teste de fiação ──────────────────────────────────────────────────
static void gpio_walk(void)
{
    int pins[]        = { GC9A01_PIN_MOSI, GC9A01_PIN_SCLK, GC9A01_PIN_CS, GC9A01_PIN_DC };
    const char *names[]= { "SDA/MOSI", "SCL/SCLK", "CS", "DC" };

    uint64_t mask = (1ULL<<GC9A01_PIN_MOSI)|(1ULL<<GC9A01_PIN_SCLK)|
                    (1ULL<<GC9A01_PIN_CS)|(1ULL<<GC9A01_PIN_DC);
    gpio_config_t io = { .pin_bit_mask = mask, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);
    for (int i = 0; i < 4; i++) gpio_set_level(pins[i], 0);

    for (int i = 0; i < 4; i++) {
        ESP_LOGW(TAG, ">>> FIACAO: GPIO%d (display pin %s) pulsando 0<->3.3V por 5s",
                 pins[i], names[i]);
        for (int k = 0; k < 10; k++) { gpio_set_level(pins[i], k & 1); vTaskDelay(pdMS_TO_TICKS(500)); }
        gpio_set_level(pins[i], 0);
    }
}

// ─── SPI setup ────────────────────────────────────────────────────────────────
static void spi_start(void)
{
    gpio_config_t io = { .pin_bit_mask = (1ULL<<GC9A01_PIN_DC), .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);

    spi_bus_config_t bus = {
        .sclk_io_num = GC9A01_PIN_SCLK, .mosi_io_num = GC9A01_PIN_MOSI,
        .miso_io_num = -1, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = GC9A01_W * 2 + 16,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(GC9A01_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 10 * 1000 * 1000,  // 10 MHz: descarta integridade de sinal
        .mode = 0, .spics_io_num = GC9A01_PIN_CS, .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(GC9A01_SPI_HOST, &dev, &s_spi));
}

// ─── Loop principal do diagnóstico ────────────────────────────────────────────
void diag_run(void)
{
    ESP_LOGW(TAG, "================ DIAGNOSTICO GC9A01 ================");
    ESP_LOGW(TAG, "Pinos: SCLK=%d MOSI=%d CS=%d DC=%d  (RST/BL=3V3)",
             GC9A01_PIN_SCLK, GC9A01_PIN_MOSI, GC9A01_PIN_CS, GC9A01_PIN_DC);

    ESP_LOGW(TAG, "----- FASE A: FIACAO (probe os pinos do DISPLAY com multimetro DC) -----");
    gpio_walk();
    gpio_walk();

    ESP_LOGW(TAG, "----- FASE B: SPI 10MHz mode0, alternando controlador/cor -----");
    spi_start();

    int round = 0;
    while (1) {
        ESP_LOGW(TAG, "===== rodada %d =====", ++round);

        ESP_LOGW(TAG, "[1] GC9A01 init + INVON");
        run_seq(GC9A01_SEQ);
        show_colors();

        ESP_LOGW(TAG, "[2] GC9A01 INVOFF (0x20)");
        lcd_cmd(0x20); vTaskDelay(pdMS_TO_TICKS(50));
        show_colors();

        ESP_LOGW(TAG, "[3] ST7789 init (se o painel for ST7789)");
        run_seq(ST7789_SEQ);
        show_colors();
    }
}
