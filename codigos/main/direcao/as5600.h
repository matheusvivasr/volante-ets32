#pragma once
#include <stdint.h>

/*
 * as5600.h – sensor de angulo magnetico AS5600 (12 bits) por I2C.
 *
 * Substitui o AS5047P (que era SPI). Ligado DIRETO na S3, 3.3V.
 *   S3 GPIO6 -> SDA   S3 GPIO7 -> SCL
 *   3V3 -> VCC        GND -> GND        DIR -> GND (fixa o sentido)
 *   (pinos OUT e GPO do modulo NAO sao usados)
 *
 * Endereco I2C fixo 0x36. Angulo bruto = RAW ANGLE (regs 0x0C/0x0D, 12 bits).
 * Status do ima = reg 0x0B (MD=detectado, ML=fraco, MH=forte). Ima diametral
 * centrado sobre o chip, gap 0.5-3 mm. Multi-turn e calibracao iguais ao AS5047
 * (so muda o CPR: 4096 em vez de 16384).
 */

#define AS5600_SDA_PIN   6
#define AS5600_SCL_PIN   7
#define AS5600_ADDR      0x36
#define AS5600_CPR       4096            // 2^12 contagens por volta

void as5600_init(void);

// Angulo bruto 0..4095. Retorna -1 em erro de I2C / sem resposta.
int32_t as5600_read_raw(void);

// Angulo em graus 0..360. Retorna -1.0f em erro.
float as5600_read_deg(void);
