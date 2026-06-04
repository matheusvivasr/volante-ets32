#pragma once
#include <stdint.h>

// OLED 0.42" embutido na placa C3 (SSD1306 72x40, I2C SDA=GPIO5 SCL=GPIO6, 0x3C).
// Usado como indicador de combustível estilo "bateria de celular antigo".

void oled_init(void);

// Desenha a bateria de combustível. pct 0..100. pct < 0 = sem sinal (bateria vazia).
void oled_fuel(int pct);
