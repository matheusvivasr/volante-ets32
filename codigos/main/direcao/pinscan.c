/*
 * pinscan.c - descobre a ordem dos fios do AS5047P + testa tolerancia/modo.
 *
 * Fase A: 24 permutacoes de (CLK,MISO,MOSI,CSn) a 100 kHz (modo 1), contando
 *         valid (frame bom) e resp (quantas leituras o chip DIRIGIU a linha,
 *         mesmo com erro). resp>0 = chip respondeu; resp=0 = contato aberto.
 * Fase B: varre os 4 modos SPI na fiacao conhecida (CSn=G4,CLK=G5,MOSI=G6,MISO=G7)
 *         p/ descartar "logica diferente".
 * Depois trava no melhor e transmite o angulo ao vivo (TAG "angle").
 */

#include "pinscan.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

static const char *TAG = "pinscan";

#define PINSCAN_HOST  SPI2_HOST
#define PINSCAN_HZ    (100 * 1000)        // 100 kHz: tolerante a fio/solda
#define CMD_ANGLECOM  0xFFFF

// Fiacao conhecida (informada na bancada):
#define FOCUS_CLK  5
#define FOCUS_MISO 7
#define FOCUS_MOSI 6
#define FOCUS_CS   4

static inline int par_even(uint16_t v) { return (__builtin_popcount(v) & 1) == 0; }

static uint16_t xfer16(spi_device_handle_t h, uint16_t out)
{
    uint8_t tx[2] = { (uint8_t)(out >> 8), (uint8_t)(out & 0xFF) };
    uint8_t rx[2] = { 0, 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_polling_transmit(h, &t);
    return ((uint16_t)rx[0] << 8) | rx[1];
}

// Monta o comando de LEITURA de um registro (R/W=1) com paridade par no bit15.
static uint16_t rd_cmd(uint16_t addr)
{
    uint16_t c = (addr & 0x3FFF) | 0x4000;       // bit14 = read
    if (__builtin_popcount(c) & 1) c |= 0x8000;  // paridade par
    return c;
}

void as5047_pinscan(void)
{
    ESP_LOGW(TAG, "=== DIAG ERRFL · fiacao CSn=G%d CLK=G%d MOSI=G%d MISO=G%d modo 1 @ %d Hz ===",
             FOCUS_CS, FOCUS_CLK, FOCUS_MOSI, FOCUS_MISO, PINSCAN_HZ);
    ESP_LOGW(TAG, "Le/limpa ERRFL (FRERR=clock, INVCOMM/PARERR=comando/MOSI) e depois o angulo.");

    spi_bus_config_t bus = {
        .sclk_io_num = FOCUS_CLK, .mosi_io_num = FOCUS_MOSI, .miso_io_num = FOCUS_MISO,
        .quadwp_io_num = -1, .quadhd_io_num = -1, .max_transfer_sz = 4,
    };
    spi_bus_initialize(PINSCAN_HOST, &bus, SPI_DMA_DISABLED);
    spi_device_interface_config_t dev = {
        .clock_speed_hz = PINSCAN_HZ, .mode = 1, .spics_io_num = FOCUS_CS, .queue_size = 1,
    };
    spi_device_handle_t h;
    spi_bus_add_device(PINSCAN_HOST, &dev, &h);
    gpio_set_pull_mode(FOCUS_MISO, GPIO_PULLUP_ONLY);

    const uint16_t CMD_RD_ERRFL = rd_cmd(0x0001);   // registro ERRFL
    while (1) {
        // pipeline: cada xfer envia 1 cmd e devolve o dado do cmd ANTERIOR
        xfer16(h, CMD_RD_ERRFL);                 // pede ERRFL
        uint16_t e  = xfer16(h, CMD_ANGLECOM);   // recebe ERRFL, pede angulo
        uint16_t a1 = xfer16(h, CMD_ANGLECOM);   // recebe angulo
        uint16_t a2 = xfer16(h, CMD_ANGLECOM);   // recebe angulo (estavel)

        int errfl   = e & 0x3FFF;
        int frerr   = errfl & 1;
        int invcomm = (errfl >> 1) & 1;
        int parerr  = (errfl >> 2) & 1;
        int aef     = (a2 >> 14) & 1;
        int raw     = a2 & 0x3FFF;
        int ok      = par_even(a2) && !aef && a2 != 0x0000 && a2 != 0xFFFF;
        ESP_LOGI("angle", "ERRFL=0x%04X [FRERR=%d INVCOMM=%d PARERR=%d]  angle=0x%04X errf=%d raw=%5d ang=%6.2f %s",
                 e, frerr, invcomm, parerr, a2, aef, raw, raw * 360.0f / 16384.0f,
                 ok ? "VALIDO" : "INVALIDO");
        (void)a1;
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}
