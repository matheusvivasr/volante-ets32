#pragma once
#include <stdint.h>

/*
 * as5047.h – sensor de angulo magnetico AS5047P (14 bits) por SPI.
 *
 * Ligado DIRETO na S3 (nucleo do FFB, nunca em no remoto). 3.3V, sem level shifter.
 *
 * SEQUENCIA DOS PINOS (header P1 do EK_AB, fileira de CIMA = pinos 1-8).
 * Ao reprogramar os #define abaixo, manter G4-G7 alinhados com estes pads.
 * ATENCAO: P1.3 = NC (pular!) -> contar o NC evita erro de off-by-one.
 *   P1.1 5V   -> (nao usar)
 *   P1.2 3V3  -> S3 3V3
 *   P1.3 NC   -> (pular)
 *   P1.4 CSn  -> S3 GPIO7   (AS5047_PIN_CS)
 *   P1.5 CLK  -> S3 GPIO4   (AS5047_PIN_SCLK)
 *   P1.6 MOSI -> S3 GPIO6   (AS5047_PIN_MOSI, DI do sensor)
 *   P1.7 MISO -> S3 GPIO5   (AS5047_PIN_MISO, DO do sensor)
 *   P1.8 GND  -> S3 GND
 *
 * ALIMENTACAO 3.3V OBRIGATORIA: os I/O seguem o VDD (VOH = VDD-0.5). A 5V o
 * MISO sairia ~4.5V e QUEIMA o GPIO5 (S3 nao e 5V-tolerante). No EK_AB: JP1 no
 * 3V3 (R2 populado / R1 fora); no chip VDD e VDD3V3 amarrados + cap 1uF.
 * Ver LIGACOES.md §7 e datasheet AS5047P DS000324.
 *
 * SPI modo 1 (CPOL=0, CPHA=1), ate 10 MHz. Frames de 16 bits, MSB first, com
 * paridade par no bit 15 e error-flag no bit 14. Ima DIAMETRAL centralizado
 * sobre o chip, gap 0.5-2mm (alinhamento mecanico critico p/ linearidade).
 */

#define AS5047_PIN_SCLK  4
#define AS5047_PIN_MISO  5
#define AS5047_PIN_MOSI  6
#define AS5047_PIN_CS    7
#define AS5047_SPI_HOST  SPI2_HOST
#define AS5047_SPI_HZ    (8 * 1000 * 1000)   // 8 MHz (margem do max 10 MHz)
#define AS5047_CPR       16384               // 2^14 contagens por volta

void as5047_init(void);

// Angulo bruto 0..16383. Retorna -1 em erro (paridade/error-flag/sem sensor).
int32_t as5047_read_raw(void);

// Angulo em graus 0..360. Retorna -1.0f em erro.
float as5047_read_deg(void);

// Diagnostico de bring-up: palavra CRUA de 16 bits do ultimo frame, SEM mascarar
// paridade/error-flag. Le a causa de falha de fiacao direto no valor:
//   0xFFFF = MISO ocioso em alto (MISO solto / CS nao seleciona / CLK nao chega)
//   0x0000 = MISO preso em baixo (curto p/ GND ou chip mudo)
// Nao usar no caminho normal de leitura.
uint16_t as5047_read_word(void);
