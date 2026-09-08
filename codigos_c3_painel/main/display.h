#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
 * Painel do volante – display GC9A01 (TFT redondo 240x240, SPI).
 *
 * Placa: ESP32-C3 "0.42 OLED" (FN4/FH4). Pinos escolhidos para NÃO conflitar
 * com o OLED embutido (GPIO5/6 I2C), USB nativo (18/19), console UART0 (20/21)
 * nem os strapping (2/8/9). Livres usados: 3,4,7,10 para o SPI e 0,1 para a UART.
 *
 * Ligação do GC9A01 (reordenada em 2026-09-08: mesmos 4 pinos, papéis trocados
 * pra fiação em linha reta — sem cruzar fio — nos pads 3,4,[5,6=OLED],7,[8,9=relé/BOOT],10):
 *   VCC -> 3V3      GND -> GND
 *   CS  -> GPIO3    DC  -> GPIO4
 *   SDA -> GPIO7    SCL -> GPIO10
 *   RST -> 3V3      BL  -> 3V3   (reset por software; backlight sempre ligado)
 */

#define GC9A01_SPI_HOST   SPI2_HOST
#define GC9A01_PIN_SCLK   10
#define GC9A01_PIN_MOSI   7
#define GC9A01_PIN_CS     3
#define GC9A01_PIN_DC     4
#define GC9A01_PIN_RST    (-1)   // -1 = sem pino de reset (usa reset por software)
#define GC9A01_PIN_BL     (-1)   // -1 = backlight fixo no 3V3
#define GC9A01_SPI_HZ     (10 * 1000 * 1000)   // 10 MHz. Borrão vertical do conteúdo na placa soldada = FALTA DE DESACOPLAMENTO no VCC/GND do display (não é clock): pôr 100nF+10uF colado (ver LIGACOES.md §5). Com o cap, 10 MHz roda limpo.

#define GC9A01_W 240
#define GC9A01_H 240

// Imagem de telemetria que o painel exibe (preenchida pelo main a partir das msgs).
typedef struct {
    float    speed_kmh;
    uint16_t rpm;
    int8_t   gear;
    uint8_t  fuel_pct;
    uint8_t  temp_c;
    uint16_t led_flags;   // bitmap 0x101
    uint32_t dist_m;      // distância ao destino (m)
    uint16_t eta_min;     // ETA (min)
    uint16_t odo_km;      // odômetro (km)
} panel_data_t;

// Views que se alternam na tela (a cada ~2s, controlado pelo main).
typedef enum {
    VIEW_VELOCIDADE = 0,  // velocidade grande + marcha + indicadores
    VIEW_MOTOR,           // RPM grande + temperatura + combustível
    VIEW_NAV,             // distância / ETA / odômetro
    VIEW_COUNT
} panel_view_t;

void display_init(void);

// Tela de boot/diagnóstico: barras de cor (verifica ordem RGB/BGR) + mapa de pinos.
// Útil na primeira ligação para validar SPI, cores e fiação.
void display_boot(void);

// Desenha a view indicada com os dados atuais.
void display_show(const panel_data_t *d, panel_view_t view);

// Tela de "sem sinal" (telemetria não chega há um tempo).
void display_no_signal(void);

// Anti-ghosting: inverte a tela (negativo) ou volta ao normal. neg=true -> negativo.
void display_invert(bool neg);

// Preenche a tela inteira com uma cor sólida (RGB565).
void display_fill(uint16_t color);

// Anti-ghosting: modo da varredura (0=faixa 120px / 1=faixa 15px rápida / 2=8 barras
// idle). Ciclado pelo BOOT longo. set() normaliza para 0..2.
void display_set_ag_mode(int m);
int  display_ag_mode(void);
