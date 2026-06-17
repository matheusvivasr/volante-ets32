/*
 * display.c – driver GC9A01 (TFT redondo 240x240 IPS, SPI) para o painel.
 *
 * Bare-metal sobre driver/spi_master (mesmo espírito do antigo SSD1306).
 * Framebuffer RGB565 completo em RAM (240*240*2 = 112.5 KB), enviado inteiro
 * a cada refresh. Fonte 5x7 escalável. Três views alternáveis pelo main.
 */

#include "display.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "leds.h"          // só para reusar as flags LED_*
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const char *TAG = "gc9a01";

// ─── Cores (RGB565) ───────────────────────────────────────────────────────────
#define RGB565(r,g,b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))
#define COL_BLACK   RGB565(0,0,0)
#define COL_WHITE   RGB565(255,255,255)
#define COL_GREEN   RGB565(0,220,80)
#define COL_RED     RGB565(255,40,40)
#define COL_AMBER   RGB565(255,170,0)
#define COL_BLUE    RGB565(60,140,255)
#define COL_GRAY    RGB565(110,110,110)
#define COL_ORANGE   RGB565(255,75,0)    // laranja com mais vermelho (menos desbotado)
#define COL_DGRAY    RGB565(55,55,55)
#define COL_BG       RGB565(24,24,28)    // fundo cinza off-black (preto 100% = luz apagada)
#define COL_OFFWHITE RGB565(200,200,192) // off-white dos riscos/numeros (≠ branco puro do arco)

#define MIN3(a,b,c) ((a)<(b)?((a)<(c)?(a):(c)):((b)<(c)?(b):(c)))
#define MAX3(a,b,c) ((a)>(b)?((a)>(c)?(a):(c)):((b)>(c)?(b):(c)))

// O GC9A01 espera RGB565 big-endian no fio; guardamos já trocado.
static inline uint16_t swap16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }

// ─── Estado SPI / framebuffer ────────────────────────────────────────────────
static spi_device_handle_t s_spi;
static uint16_t *s_fb;   // 240*240, big-endian

// ─── SPI low-level ────────────────────────────────────────────────────────────
static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(GC9A01_PIN_DC, 0);
    spi_transaction_t t = { .length = 8, .tx_buffer = &cmd };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void lcd_data(const uint8_t *data, int len)
{
    if (len <= 0) return;
    gpio_set_level(GC9A01_PIN_DC, 1);
    spi_transaction_t t = { .length = (size_t)len * 8, .tx_buffer = data };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void lcd_data1(uint8_t d) { lcd_data(&d, 1); }

// ─── Sequência de init do GC9A01 ─────────────────────────────────────────────
typedef struct { uint8_t cmd; uint8_t len; uint8_t data[16]; uint16_t delay_ms; } gc_cmd_t;

static const gc_cmd_t INIT_SEQ[] = {
    {0xEF, 0, {0}, 0},
    {0xEB, 1, {0x14}, 0},
    {0xFE, 0, {0}, 0},
    {0xEF, 0, {0}, 0},
    {0xEB, 1, {0x14}, 0},
    {0x84, 1, {0x40}, 0},
    {0x85, 1, {0xFF}, 0},
    {0x86, 1, {0xFF}, 0},
    {0x87, 1, {0xFF}, 0},
    {0x88, 1, {0x0A}, 0},
    {0x89, 1, {0x21}, 0},
    {0x8A, 1, {0x00}, 0},
    {0x8B, 1, {0x80}, 0},
    {0x8C, 1, {0x01}, 0},
    {0x8D, 1, {0x01}, 0},
    {0x8E, 1, {0xFF}, 0},
    {0x8F, 1, {0xFF}, 0},
    {0xB6, 2, {0x00, 0x20}, 0},
    {0x36, 1, {0x08}, 0},   // MADCTL: BGR, sem espelho. 0x40/0x48 tinham MX (espelhava texto)
    {0x3A, 1, {0x05}, 0},   // COLMOD: 16 bits/pixel
    {0x90, 4, {0x08, 0x08, 0x08, 0x08}, 0},
    {0xBD, 1, {0x06}, 0},
    {0xBC, 1, {0x00}, 0},
    {0xFF, 3, {0x60, 0x01, 0x04}, 0},
    {0xC3, 1, {0x13}, 0},
    {0xC4, 1, {0x13}, 0},
    {0xC9, 1, {0x22}, 0},
    {0xBE, 1, {0x11}, 0},
    {0xE1, 2, {0x10, 0x0E}, 0},
    {0xDF, 3, {0x21, 0x0C, 0x02}, 0},
    {0xF0, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 0},
    {0xF1, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 0},
    {0xF2, 6, {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A}, 0},
    {0xF3, 6, {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F}, 0},
    {0xED, 2, {0x1B, 0x0B}, 0},
    {0xAE, 1, {0x77}, 0},
    {0xCD, 1, {0x63}, 0},
    {0x70, 9, {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 0},
    {0xE8, 1, {0x34}, 0},
    {0x62, 12, {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70}, 0},
    {0x63, 12, {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70}, 0},
    {0x64, 7, {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07}, 0},
    {0x66, 10, {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00}, 0},
    {0x67, 10, {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98}, 0},
    {0x74, 7, {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00}, 0},
    {0x98, 2, {0x3E, 0x07}, 0},
    {0x35, 0, {0}, 0},      // tearing effect on
    {0x21, 0, {0}, 0},      // inversão ON (fundo preto correto; INVOFF deixava tudo invertido)
    {0x11, 0, {0}, 120},    // sleep out
    {0x29, 0, {0}, 20},     // display on
    {0xFF, 0xFF, {0}, 0},   // fim
};

static void lcd_set_window(int x0, int y0, int x1, int y1)
{
    uint8_t d[4];
    lcd_cmd(0x2A); d[0] = x0 >> 8; d[1] = x0 & 0xFF; d[2] = x1 >> 8; d[3] = x1 & 0xFF; lcd_data(d, 4);
    lcd_cmd(0x2B); d[0] = y0 >> 8; d[1] = y0 & 0xFF; d[2] = y1 >> 8; d[3] = y1 & 0xFF; lcd_data(d, 4);
    lcd_cmd(0x2C);
}

// Envia o framebuffer em blocos de linhas (um único transfer de 115KB estoura o SPI).
#define FLUSH_CHUNK_ROWS 24

// ── Anti-ghosting com 3 MODOS (ciclados pelo BOOT longo), aplicado em TODO flush:
//   0 = faixa de 120px em NEGATIVO (50% duty, lenta) -> proteção máxima ("sem sinal").
//   1 = faixa de 15px em NEGATIVO, rápida -> discreta, p/ uso com sinal (mostradores).
//   2 = 8 barras de cor (R G B W + negativos C M Y K) rolando -> modo idle/varredura.
// Modos 0/1 INVERTEM (overlay) sobre o conteúdo; o modo 2 SUBSTITUI por barras (o
// conteúdo é redesenhado pelo render_task no frame seguinte). Roda 100% do tempo.
#define AG_BARS  8
static volatile int s_ag_mode = 0;   // 0=faixa120 (padrão) | 1=faixa15 | 2=barras
static int s_ag_y = 0;

void display_set_ag_mode(int m) { m = ((m % 3) + 3) % 3; if (m != s_ag_mode) { s_ag_mode = m; s_ag_y = 0; } }
int  display_ag_mode(void)      { return s_ag_mode; }

// Inverte (one's complement) h linhas a partir de s_ag_y, com wrap.
static void ag_band_xor(int h)
{
    for (int i = 0; i < h; i++) {
        int y = s_ag_y + i;
        if (y >= GC9A01_H) y -= GC9A01_H;
        uint16_t *row = &s_fb[y * GC9A01_W];
        for (int x = 0; x < GC9A01_W; x++) row[x] ^= 0xFFFF;   // negativo (qualquer byte order)
    }
}

// Modo 2: sobrescreve a tela com 8 barras de cor rolando (R G B W e seus negativos).
static void ag_bars_fill(void)
{
    static const uint16_t bars[AG_BARS] = {
        0xF800, 0x07E0, 0x001F, 0xFFFF,   // R, G, B, W
        0x07FF, 0xF81F, 0xFFE0, 0x0000,   // ciano(~R), magenta(~G), amarelo(~B), preto(~W)
    };
    int bh = GC9A01_H / AG_BARS;          // 240/8 = 30 px por barra
    for (int y = 0; y < GC9A01_H; y++) {
        int yy = y + s_ag_y; if (yy >= GC9A01_H) yy -= GC9A01_H;
        uint16_t c = swap16(bars[(yy / bh) & (AG_BARS - 1)]);   // s_fb é big-endian
        uint16_t *row = &s_fb[y * GC9A01_W];
        for (int x = 0; x < GC9A01_W; x++) row[x] = c;
    }
}

static void lcd_flush(void)
{
    int mode = s_ag_mode, step;            // captura o modo p/ aplicar/restaurar coerente
    if (mode == 0)      { ag_band_xor(120); step = 2;  }   // faixa 120px lenta
    else if (mode == 1) { ag_band_xor(15);  step = 10; }   // faixa 15px rápida
    else                { ag_bars_fill();   step = 2;  }   // 8 barras (idle)

    lcd_set_window(0, 0, GC9A01_W - 1, GC9A01_H - 1);
    gpio_set_level(GC9A01_PIN_DC, 1);
    for (int y = 0; y < GC9A01_H; y += FLUSH_CHUNK_ROWS) {
        int n = (y + FLUSH_CHUNK_ROWS <= GC9A01_H) ? FLUSH_CHUNK_ROWS : (GC9A01_H - y);
        spi_transaction_t t = {
            .length = (size_t)GC9A01_W * n * 16,
            .tx_buffer = &s_fb[y * GC9A01_W],
        };
        ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));   // bloqueante: DMA pronto ao retornar
    }

    if (mode == 0)      ag_band_xor(120);   // restaura o conteúdo (overlay dos modos 0/1)
    else if (mode == 1) ag_band_xor(15);
    // modo 2 não restaura (barras redesenhadas a cada flush; conteúdo volta ao sair do modo)
    s_ag_y += step;
    if (s_ag_y >= GC9A01_H) s_ag_y -= GC9A01_H;
}

// ─── Fonte 5x7 (colunas, bit0 = topo) ────────────────────────────────────────
typedef struct { char c; uint8_t col[5]; } glyph_t;
static const glyph_t FONT[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}},
    {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x42,0x61,0x51,0x49,0x46}},
    {'3', {0x21,0x41,0x45,0x4B,0x31}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}},
    {'5', {0x27,0x45,0x45,0x45,0x39}},
    {'6', {0x3C,0x4A,0x49,0x49,0x30}},
    {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x06,0x49,0x49,0x29,0x1E}},
    {'.', {0x00,0x60,0x60,0x00,0x00}},
    {'/', {0x20,0x10,0x08,0x04,0x02}},
    {':', {0x00,0x36,0x36,0x00,0x00}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},
    {'%', {0x23,0x13,0x08,0x64,0x62}},
    {'A', {0x7E,0x11,0x11,0x11,0x7E}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'D', {0x7F,0x41,0x41,0x22,0x1C}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}},
    {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x00,0x41,0x7F,0x41,0x00}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},
    {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x0C,0x02,0x7F}},
    {'N', {0x7F,0x04,0x08,0x10,0x7F}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}},
    {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}},
    {'V', {0x1F,0x20,0x40,0x20,0x1F}},
    {'B', {0x7F,0x49,0x49,0x49,0x36}},
    {'W', {0x7F,0x20,0x18,0x20,0x7F}},
    {'X', {0x63,0x14,0x08,0x14,0x63}},
    {'J', {0x20,0x40,0x41,0x3F,0x01}},
    {'Q', {0x3E,0x41,0x51,0x21,0x5E}},
    {'Y', {0x03,0x04,0x78,0x04,0x03}},
    {'Z', {0x61,0x51,0x49,0x45,0x43}},
};

static const uint8_t *glyph_of(char c)
{
    if (c >= 'a' && c <= 'z') c -= 32;   // fonte é só maiúscula; aceita minúscula
    for (size_t i = 0; i < sizeof(FONT)/sizeof(FONT[0]); i++)
        if (FONT[i].c == c) return FONT[i].col;
    return FONT[0].col; // espaço
}

// ─── Primitivas de desenho no framebuffer ────────────────────────────────────
static void fb_clear(uint16_t color)
{
    uint16_t v = swap16(color);
    for (int i = 0; i < GC9A01_W * GC9A01_H; i++) s_fb[i] = v;
}

static inline void fb_pixel(int x, int y, uint16_t color)
{
    if ((unsigned)x >= GC9A01_W || (unsigned)y >= GC9A01_H) return;
    s_fb[y * GC9A01_W + x] = swap16(color);
}

static void fb_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            fb_pixel(x + i, y + j, color);
}

static int g_bold = 1;   // 1 = engrossa a fonte (negrito) em +1px por traço

// largura em px de uma string no scale dado (5 col + 1 espaço por char)
static int text_w(const char *s, int scale) { return (int)strlen(s) * (6 * scale + g_bold); }

static void fb_char(int x, int y, char c, int scale, uint16_t fg)
{
    const uint8_t *g = glyph_of(c);
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                fb_fill_rect(x + col * scale, y + row * scale, scale, scale, fg);
                if (g_bold) fb_fill_rect(x + col * scale + scale, y + row * scale, 1, scale, fg);
            }
        }
    }
}

static void fb_text(int x, int y, const char *s, int scale, uint16_t fg)
{
    while (*s) { fb_char(x, y, *s, scale, fg); x += 6 * scale + g_bold; s++; }
}

// texto centrado horizontalmente em torno de cx
static void fb_text_c(int cx, int y, const char *s, int scale, uint16_t fg)
{
    fb_text(cx - text_w(s, scale) / 2, y, s, scale, fg);
}

// ─── Primitivas geométricas ──────────────────────────────────────────────────
#define GA_CX   120      // centro do mostrador
#define GA_CY   120
#define GA_START 180.0f  // minimo a esquerda (sobre o diametro)
#define GA_SWEEP 175.0f  // escala vai ate ~175°; o arco fecha 180° por fora dela

static inline float d2r(float deg) { return deg * 0.0174532925f; }

// valor -> angulo no mostrador
static float val_angle(float v, float vmin, float vmax)
{
    if (v < vmin) v = vmin;
    if (v > vmax) v = vmax;
    return GA_START + (v - vmin) / (vmax - vmin) * GA_SWEEP;
}

static void fb_line(int x0, int y0, int x1, int y1, uint16_t c)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb_pixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void fb_disc(int cx, int cy, int r, uint16_t c)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) fb_pixel(cx + x, cy + y, c);
}

// contorno de circulo (midpoint)
static void fb_circle(int cx, int cy, int r, uint16_t c)
{
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        fb_pixel(cx + x, cy + y, c); fb_pixel(cx - x, cy + y, c);
        fb_pixel(cx + x, cy - y, c); fb_pixel(cx - x, cy - y, c);
        fb_pixel(cx + y, cy + x, c); fb_pixel(cx - y, cy + x, c);
        fb_pixel(cx + y, cy - x, c); fb_pixel(cx - y, cy - x, c);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

// arco (parte de circulo) entre os angulos a0..a1 (graus), centrado no mostrador
static void fb_arc(int r, float a0, float a1, uint16_t c)
{
    int steps = (int)((a1 - a0) * 4) + 1;
    for (int i = 0; i <= steps; i++) {
        float a = d2r(a0 + (a1 - a0) * i / steps);
        fb_pixel(GA_CX + (int)(r * cosf(a) + 0.5f), GA_CY + (int)(r * sinf(a) + 0.5f), c);
    }
}

static void fb_tri(int x0,int y0,int x1,int y1,int x2,int y2, uint16_t c)
{
    int minx = MIN3(x0,x1,x2), maxx = MAX3(x0,x1,x2);
    int miny = MIN3(y0,y1,y2), maxy = MAX3(y0,y1,y2);
    for (int y = miny; y <= maxy; y++)
        for (int x = minx; x <= maxx; x++) {
            int w0 = (x1-x0)*(y-y0) - (y1-y0)*(x-x0);
            int w1 = (x2-x1)*(y-y1) - (y2-y1)*(x-x1);
            int w2 = (x0-x2)*(y-y2) - (y0-y2)*(x-x2);
            if ((w0>=0 && w1>=0 && w2>=0) || (w0<=0 && w1<=0 && w2<=0))
                fb_pixel(x, y, c);
        }
}

// ponteiro: do centro ate o raio 'len', com leve afunilamento
static void fb_needle(float ang_deg, int len, int half_w, uint16_t c)
{
    float a = d2r(ang_deg), ca = cosf(a), sa = sinf(a);
    float px = -sa, py = ca;  // perpendicular
    int tx = GA_CX + (int)(len * ca), ty = GA_CY + (int)(len * sa);
    for (int w = -half_w; w <= half_w; w++) {
        int bx = GA_CX + (int)(w * px),       by = GA_CY + (int)(w * py);
        int ex = tx   + (int)(w * 0.25f * px), ey = ty   + (int)(w * 0.25f * py);
        fb_line(bx, by, ex, ey, c);
    }
}

// risco radial do mostrador (de 'inner' a 'outer'), espessura 'width' px
static void dial_tick(float v, float vmin, float vmax, int inner, int outer, int width, uint16_t c)
{
    float a = d2r(val_angle(v, vmin, vmax)), ca = cosf(a), sa = sinf(a);
    float px = -sa, py = ca;   // perpendicular ao raio
    for (int w = 0; w < width; w++) {
        int ox = (int)(w * px), oy = (int)(w * py);
        fb_line(GA_CX + (int)(inner*ca) + ox, GA_CY + (int)(inner*sa) + oy,
                GA_CX + (int)(outer*ca) + ox, GA_CY + (int)(outer*sa) + oy, c);
    }
}

// numero do mostrador, centrado no raio 'rad'
static void dial_num(float v, float vmin, float vmax, int rad, const char *s, int scale, uint16_t c)
{
    float a = d2r(val_angle(v, vmin, vmax));
    int x = GA_CX + (int)(rad * cosf(a)), y = GA_CY + (int)(rad * sinf(a));
    fb_text_c(x, y - 3 * scale, s, scale, c);
}

// ─── Regua comum dos mostradores: 8 divisoes (9 riscos), 180° no topo ────────
// Mesmas marcacoes/espacamento p/ velocimetro e tacometro; so muda o vmax/numeros.
// Moldura do mostrador: face preta pura (semicirculo de cima) + "D" branco
// (arco 0-180° fechado por um diametro). Anel preto fica entre o arco e o anel
// cinza da borda (desenhado no display_show).
// arco centrado em (cx,cy) arbitrario (p/ os cantos arredondados da base)
static void arc_at(int cx, int cy, int r, float a0, float a1, uint16_t c)
{
    int steps = (int)((a1 - a0) * 4) + 1;
    for (int i = 0; i <= steps; i++) {
        float a = d2r(a0 + (a1 - a0) * i / steps);
        fb_pixel(cx + (int)(r * cosf(a) + 0.5f), cy + (int)(r * sinf(a) + 0.5f), c);
    }
}

// Moldura "D corpulento": face preta = semicirculo (cima) + retangulo da largura
// do diametro (baixo) com base levemente arredondada. Contorno branco. Anel preto
// dando a volta COMPLETA p/ destacar o anel cinza da borda.
static void draw_gauge_frame(void)
{
    const int cx = GA_CX, cy = GA_CY, R = 108, H = 18, CR = 8;  // lip curto (ate a base do ponteiro)

    // anel preto cheio (volta completa) p/ destacar o anel cinza
    for (int y = -117; y <= 117; y++)
        for (int x = -117; x <= 117; x++) {
            int d = x * x + y * y;
            if (d <= 117 * 117 && d >= 110 * 110) fb_pixel(cx + x, cy + y, COL_BLACK);
        }

    // face preta: semicirculo de cima + lip curto de baixo (cantos arredondados)
    for (int y = -R; y <= 0; y++)
        for (int x = -R; x <= R; x++)
            if (x * x + y * y <= R * R) fb_pixel(cx + x, cy + y, COL_BLACK);
    for (int dy = 1; dy <= H; dy++)
        for (int x = -R; x <= R; x++) {
            int ax = x < 0 ? -x : x;
            if (dy > H - CR && ax > R - CR) {
                int ex = ax - (R - CR), ey = dy - (H - CR);
                if (ex * ex + ey * ey > CR * CR) continue;
            }
            fb_pixel(cx + x, cy + dy, COL_BLACK);
        }

    // contorno branco, espessura 2px UNIFORME (sem linha de diametro)
    fb_arc(R,     GA_START, GA_START + 180.0f, COL_WHITE);            // topo
    fb_arc(R + 1, GA_START, GA_START + 180.0f, COL_WHITE);
    fb_line(cx - R,   cy, cx - R,   cy + H - CR, COL_WHITE);          // lado esq
    fb_line(cx - R+1, cy, cx - R+1, cy + H - CR, COL_WHITE);
    fb_line(cx + R,   cy, cx + R,   cy + H - CR, COL_WHITE);          // lado dir
    fb_line(cx + R-1, cy, cx + R-1, cy + H - CR, COL_WHITE);
    fb_line(cx - (R-CR), cy + H,   cx + (R-CR), cy + H,   COL_WHITE); // base
    fb_line(cx - (R-CR), cy + H-1, cx + (R-CR), cy + H-1, COL_WHITE);
    arc_at(cx - (R-CR), cy + (H-CR), CR,   90, 180, COL_WHITE);       // canto inf esq
    arc_at(cx - (R-CR), cy + (H-CR), CR-1, 90, 180, COL_WHITE);
    arc_at(cx + (R-CR), cy + (H-CR), CR,    0,  90, COL_WHITE);       // canto inf dir
    arc_at(cx + (R-CR), cy + (H-CR), CR-1,  0,  90, COL_WHITE);
}

// Régua comum (escala): pula o "0" (subentendido). Riscos off-white/vermelho.
static void draw_ruler(int vmax, int redline_idx, int label_every)
{
    // riscos secundarios: cor da zona, 1px, curtos
    for (int i = 0; i < 8; i++) {
        float v = vmax * (i + 0.5f) / 8.0f;
        uint16_t c = (redline_idx >= 0 && (i + 0.5f) >= redline_idx) ? COL_RED : COL_OFFWHITE;
        dial_tick(v, 0, vmax, 99, 107, 1, c);
    }
    // riscos principais com numero (i=1..8; o 0 fica subentendido): 2px, altos
    for (int i = 1; i <= 8; i++) {
        float v = vmax * (float)i / 8.0f;
        uint16_t c = (redline_idx >= 0 && i >= redline_idx) ? COL_RED : COL_OFFWHITE;
        dial_tick(v, 0, vmax, 90, 107, 2, c);
        if (i % label_every == 0) {
            char b[8];
            snprintf(b, sizeof(b), "%d", (int)(v + 0.5f));
            dial_num(v, 0, vmax, 76, b, 1, c);
        }
    }
}

// Numero grande + unidade pequena, agrupados e centralizados (numero a esq,
// unidade a dir alinhada na base) — versao simples da ideia de "celulas".
static void draw_value_unit(int y, const char *num, const char *unit, uint16_t numc, uint16_t unitc)
{
    int nw = text_w(num, 3), uw = text_w(unit, 1), gap = 5;
    int x = GA_CX - (nw + gap + uw) / 2;
    fb_text(x, y, num, 3, numc);
    fb_text(x + nw + gap, y + 14, unit, 1, unitc);
}

// cor padrao do combustivel (mesma regra em todas as telas)
static uint16_t fuel_color(int pct)
{
    if (pct <= 15) return COL_RED;
    if (pct <= 35) return COL_AMBER;
    return COL_GREEN;
}

// anel preto cheio (volta completa) p/ destacar o anel cinza da borda
static void draw_black_ring(void)
{
    for (int y = -117; y <= 117; y++)
        for (int x = -117; x <= 117; x++) {
            int d = x * x + y * y;
            if (d <= 117 * 117 && d >= 110 * 110) fb_pixel(GA_CX + x, GA_CY + y, COL_BLACK);
        }
}

// ─── Indicadores (telltales no semicirculo inferior) ─────────────────────────
// Sempre desenhados: apagados em PRETO (silhueta no fundo cinza), acesos coloridos.
static void draw_indicators(const panel_data_t *d)
{
    int y = 206;
    int blink = (esp_timer_get_time() / 450000) & 1;   // ~1.1 Hz (setas piscam em ambar)
    uint16_t on_l = ((d->led_flags & (LED_SETA_ESQ | LED_PISCA_ALERT)) && blink) ? COL_AMBER : COL_BLACK;
    uint16_t on_r = ((d->led_flags & (LED_SETA_DIR | LED_PISCA_ALERT)) && blink) ? COL_AMBER : COL_BLACK;
    fb_tri(GA_CX - 22, y, GA_CX - 14, y - 5, GA_CX - 14, y + 5, on_l);   // seta esq
    fb_tri(GA_CX + 22, y, GA_CX + 14, y - 5, GA_CX + 14, y + 5, on_r);   // seta dir

    uint16_t farol = COL_BLACK;
    if (d->led_flags & LED_FAROL_ALTO)       farol = COL_BLUE;
    else if (d->led_flags & LED_FAROL_BAIXO) farol = RGB565(0,90,180);
    fb_disc(GA_CX, y, 3, farol);

    fb_text_c(GA_CX - 50, y - 3, "P", 1, (d->led_flags & LED_FREIO_MAO) ? COL_RED   : COL_BLACK);
    fb_text_c(GA_CX + 50, y - 3, "F", 1, (d->led_flags & LED_LOW_FUEL)  ? COL_AMBER : COL_BLACK);
}

// ─── Views (mostradores analogicos, 180°) ────────────────────────────────────
static void view_velocidade(const panel_data_t *d)
{
    char buf[16];
    draw_gauge_frame();
    draw_ruler(160, -1, 1);   // 0..160 de 20 em 20 (mesma regua do tacometro)
    fb_needle(val_angle(d->speed_kmh, 0, 160), 104, 4, COL_ORANGE);
    fb_disc(GA_CX, GA_CY, 7, COL_WHITE);
    fb_disc(GA_CX, GA_CY, 3, COL_ORANGE);

    // leitura digital FORA do D (abaixo): numero + unidade ao lado
    snprintf(buf, sizeof(buf), "%d", (int)(d->speed_kmh + 0.5f));
    draw_value_unit(150, buf, "KM/H", COL_WHITE, COL_GRAY);
    if      (d->gear == 0)  snprintf(buf, sizeof(buf), "N");
    else if (d->gear == -1) snprintf(buf, sizeof(buf), "R");
    else                    snprintf(buf, sizeof(buf), "M%d", d->gear);
    fb_text_c(GA_CX, 180, buf, 2, COL_GRAY);   // marcha discreta (cinza)

    draw_indicators(d);
}

static void view_motor(const panel_data_t *d)
{
    char buf[16];
    float rpmk = d->rpm / 1000.0f;
    draw_gauge_frame();
    draw_ruler(8, 6, 1);      // 0..8 x1000, numeros 0..8, zona vermelha 6-8
    uint16_t nc = (rpmk >= 6) ? COL_RED : COL_BLUE;   // ponteiro azul (vermelho no redline)
    fb_needle(val_angle(rpmk, 0, 8), 104, 4, nc);
    fb_disc(GA_CX, GA_CY, 7, COL_WHITE);
    fb_disc(GA_CX, GA_CY, 3, nc);

    snprintf(buf, sizeof(buf), "%d", d->rpm);
    draw_value_unit(150, buf, "RPM", COL_WHITE, COL_GRAY);
    snprintf(buf, sizeof(buf), "T %dC", d->temp_c);
    fb_text_c(GA_CX - 38, 182, buf, 1, (d->temp_c > 100) ? COL_RED : COL_GRAY);
    snprintf(buf, sizeof(buf), "F %d%%", d->fuel_pct);
    fb_text_c(GA_CX + 38, 182, buf, 1, fuel_color(d->fuel_pct));

    draw_indicators(d);
}

static void view_nav(const panel_data_t *d)
{
    char buf[16];
    draw_black_ring();   // moldura minimalista (alem do anel cinza do display_show)

    // cruz divisoria (dentro do anel)
    fb_line(GA_CX, 46, GA_CX, 194, COL_DGRAY);
    fb_line(46, GA_CY, 194, GA_CY, COL_DGRAY);

    // centros das celulas puxados pro centro (raio, nao quadrado)
    const int lx = 74, rx = 166, ty = 66, by = 152;

    // sup-esq: DISTANCIA
    if (d->dist_m >= 1000) snprintf(buf, sizeof(buf), "%d.%dKM", (int)(d->dist_m/1000), (int)((d->dist_m%1000)/100));
    else                   snprintf(buf, sizeof(buf), "%dM", (int)d->dist_m);
    fb_text_c(lx, ty, "DISTANCIA", 1, COL_GRAY);
    fb_text_c(lx, ty + 16, buf, 2, COL_BLUE);

    // sup-dir: TEMPO ate o destino (HHhMM)
    snprintf(buf, sizeof(buf), "%02dH%02d", d->eta_min / 60, d->eta_min % 60);
    fb_text_c(rx, ty, "TEMPO", 1, COL_GRAY);
    fb_text_c(rx, ty + 16, buf, 2, COL_AMBER);

    // inf-esq: ODOMETRO
    snprintf(buf, sizeof(buf), "%dKM", d->odo_km);
    fb_text_c(lx, by, "ODOMETRO", 1, COL_GRAY);
    fb_text_c(lx, by + 16, buf, 2, COL_OFFWHITE);

    // inf-dir: COMBUSTIVEL
    snprintf(buf, sizeof(buf), "%d%%", d->fuel_pct);
    fb_text_c(rx, by, "COMBUSTIVEL", 1, COL_GRAY);
    fb_text_c(rx, by + 16, buf, 2, fuel_color(d->fuel_pct));
}

// ─── API pública ──────────────────────────────────────────────────────────────
void display_init(void)
{
    s_fb = heap_caps_malloc(GC9A01_W * GC9A01_H * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!s_fb) {
        ESP_LOGE(TAG, "sem RAM DMA para o framebuffer (%d B)", GC9A01_W * GC9A01_H * 2);
        return;
    }

    uint64_t pin_mask = (1ULL << GC9A01_PIN_DC);
#if (GC9A01_PIN_RST >= 0)
    pin_mask |= (1ULL << GC9A01_PIN_RST);
#endif
#if (GC9A01_PIN_BL >= 0)
    pin_mask |= (1ULL << GC9A01_PIN_BL);
#endif
    gpio_config_t io = { .pin_bit_mask = pin_mask, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&io);

#if (GC9A01_PIN_RST >= 0)
    gpio_set_level(GC9A01_PIN_RST, 0); vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(GC9A01_PIN_RST, 1); vTaskDelay(pdMS_TO_TICKS(120));
#endif

    spi_bus_config_t bus = {
        .sclk_io_num = GC9A01_PIN_SCLK,
        .mosi_io_num = GC9A01_PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GC9A01_W * FLUSH_CHUNK_ROWS * 2 + 64,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(GC9A01_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = GC9A01_SPI_HZ,
        .mode = 0,
        .spics_io_num = GC9A01_PIN_CS,
        .queue_size = 7,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(GC9A01_SPI_HOST, &dev, &s_spi));

    // reset por software se não houver pino de RST
    if (GC9A01_PIN_RST < 0) { lcd_cmd(0x01); vTaskDelay(pdMS_TO_TICKS(120)); }

    for (const gc_cmd_t *c = INIT_SEQ; c->cmd != 0xFF || c->len != 0xFF; c++) {
        lcd_cmd(c->cmd);
        for (int i = 0; i < c->len; i++) lcd_data1(c->data[i]);
        if (c->delay_ms) vTaskDelay(pdMS_TO_TICKS(c->delay_ms));
    }

#if (GC9A01_PIN_BL >= 0)
    gpio_set_level(GC9A01_PIN_BL, 1);
#endif

    fb_clear(COL_BLACK);
    lcd_flush();
    ESP_LOGI(TAG, "GC9A01 init OK (SCLK=%d MOSI=%d CS=%d DC=%d)",
             GC9A01_PIN_SCLK, GC9A01_PIN_MOSI, GC9A01_PIN_CS, GC9A01_PIN_DC);
}

void display_boot(void)
{
    if (!s_fb) return;

    // Barras verticais estilo "TV fora do ar" (SMPTE 75%) = tela de carregamento.
    static const uint16_t bars[] = {
        RGB565(192,192,192), // cinza
        RGB565(192,192,0),   // amarelo
        RGB565(0,192,192),   // ciano
        RGB565(0,192,0),     // verde
        RGB565(192,0,192),   // magenta
        RGB565(192,0,0),     // vermelho
        RGB565(0,0,192),     // azul
    };
    int n = 7, bw = GC9A01_W / n;
    for (int i = 0; i < n; i++) {
        int w = (i == n - 1) ? (GC9A01_W - i * bw) : bw;
        fb_fill_rect(i * bw, 0, w, GC9A01_H, bars[i]);
    }
    // faixa preta embaixo com o aviso de carregando
    fb_fill_rect(0, 172, GC9A01_W, GC9A01_H - 172, COL_BLACK);
    fb_text_c(GC9A01_W / 2, 190, "CARREGANDO", 2, COL_WHITE);

    lcd_flush();
}

void display_show(const panel_data_t *d, panel_view_t view)
{
    if (!s_fb) return;
    fb_clear(COL_BG);
    fb_circle(GA_CX, GA_CY, 119, RGB565(90,90,105));   // contorno externo do visor
    fb_circle(GA_CX, GA_CY, 118, RGB565(90,90,105));
    switch (view) {
        case VIEW_MOTOR: view_motor(d); break;
        case VIEW_NAV:   view_nav(d);   break;
        case VIEW_VELOCIDADE:
        default:         view_velocidade(d); break;
    }
    lcd_flush();
}

void display_no_signal(void)
{
    if (!s_fb) return;
    fb_clear(COL_BG);
    fb_text_c(GC9A01_W / 2, 90, "SEM", 4, COL_GRAY);
    fb_text_c(GC9A01_W / 2, 130, "SINAL", 4, COL_GRAY);
    lcd_flush();
}

void display_fill(uint16_t color)
{
    if (!s_fb) return;
    fb_clear(color);
    lcd_flush();
}

void display_invert(bool neg)
{
    // INIT usa INVON (0x21) p/ cores corretas; 0x20 (INVOFF) mostra o NEGATIVO.
    // Pulso periodico de negativo balanceia o DC e evita retencao de imagem.
    lcd_cmd(neg ? 0x20 : 0x21);
}
