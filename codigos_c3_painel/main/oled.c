/*
 * oled.c – OLED 0.42" embutido na C3 (SSD1306 72x40, I2C) como medidor de
 * combustível estilo "bateria de celular antigo": um contorno de bateria com
 * N segmentos que se apagam um a um conforme o nível cai.
 *
 * I2C: SDA=GPIO5, SCL=GPIO6, addr 0x3C (livres; o TFT usa SPI 3/4/7/10).
 * Se a imagem sair deslocada na horizontal, ajuste OLED_COL_OFFSET.
 */

#include "oled.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include <string.h>

#define OLED_SDA        5
#define OLED_SCL        6
#define OLED_ADDR       0x3C
#define OLED_W          72
#define OLED_H          40
#define OLED_PAGES      (OLED_H / 8)   // 5
#define OLED_COL_OFFSET 28             // 72x40 mapeia a partir da coluna 28 da GDDRAM
#define OLED_SEGMENTS   5              // nº de barras da bateria

static uint8_t s_fb[OLED_W * OLED_PAGES];

// ─── I2C low-level ────────────────────────────────────────────────────────────
static void oled_cmd(uint8_t c)
{
    uint8_t b[2] = {0x00, c};
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(h, b, 2, true);
    i2c_master_stop(h);
    i2c_master_cmd_begin(I2C_NUM_0, h, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(h);
}

static void oled_flush(void)
{
    oled_cmd(0x21); oled_cmd(OLED_COL_OFFSET); oled_cmd(OLED_COL_OFFSET + OLED_W - 1);
    oled_cmd(0x22); oled_cmd(0); oled_cmd(OLED_PAGES - 1);

    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, (OLED_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, 0x40, true);
    i2c_master_write(h, s_fb, sizeof(s_fb), true);
    i2c_master_stop(h);
    i2c_master_cmd_begin(I2C_NUM_0, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);
}

// ─── Primitivas no framebuffer ───────────────────────────────────────────────
static void px(int x, int y, int on)
{
    if ((unsigned)x >= OLED_W || (unsigned)y >= OLED_H) return;
    int page = y / 8, bit = y % 8;
    if (on) s_fb[page * OLED_W + x] |=  (1 << bit);
    else    s_fb[page * OLED_W + x] &= ~(1 << bit);
}
static void fill(int x, int y, int w, int h, int on)
{
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) px(x + i, y + j, on);
}
static void frame(int x, int y, int w, int h)
{
    for (int i = 0; i < w; i++) { px(x + i, y, 1); px(x + i, y + h - 1, 1); }
    for (int j = 0; j < h; j++) { px(x, y + j, 1); px(x + w - 1, y + j, 1); }
}

// ─── API ──────────────────────────────────────────────────────────────────────
void oled_init(void)
{
    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = OLED_SDA,
        .scl_io_num       = OLED_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    i2c_param_config(I2C_NUM_0, &cfg);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    static const uint8_t init[] = {
        0xAE,       // off
        0xD5, 0x80, // clock
        0xA8, 0x27, // multiplex = 39 (40 linhas)
        0xD3, 0x00, // offset
        0x40,       // start line
        0x8D, 0x14, // charge pump
        0x20, 0x00, // horizontal addressing
        0xA1,       // seg remap
        0xC8,       // com scan dec
        0xDA, 0x12, // com pins
        0x81, 0x9F, // contrast
        0xD9, 0xF1, // precharge
        0xDB, 0x40, // vcomh
        0xA4,       // resume
        0xA6,       // normal
        0xAF,       // on
    };
    for (size_t i = 0; i < sizeof(init); i++) oled_cmd(init[i]);

    memset(s_fb, 0, sizeof(s_fb));
    oled_flush();
    ESP_LOGI("oled", "SSD1306 0.42 init (SDA=%d SCL=%d)", OLED_SDA, OLED_SCL);
}

void oled_fuel(int pct)
{
    memset(s_fb, 0, sizeof(s_fb));

    // corpo da bateria + terminal (nub) à direita
    int bx = 4, by = 9, bw = 56, bh = 22;
    frame(bx, by, bw, bh);
    fill(bx + bw, by + 6, 4, 10, 1);

    // segmentos internos que se apagam um a um
    int inx = bx + 3, iny = by + 3, inw = bw - 6, inh = bh - 6;
    int filled = (pct < 0) ? 0 : ((pct * OLED_SEGMENTS + 50) / 100);
    if (pct > 0 && filled < 1) filled = 1;
    if (filled > OLED_SEGMENTS) filled = OLED_SEGMENTS;

    int segw = (inw - (OLED_SEGMENTS - 1)) / OLED_SEGMENTS;
    for (int s = 0; s < filled; s++)
        fill(inx + s * (segw + 1), iny, segw, inh, 1);

    oled_flush();
}
