/*
 * as5047.c – driver SPI do sensor de angulo AS5047P (14 bits).
 *
 * Leitura pipelined: cada transacao de 16 bits envia o comando ANGLECOM e
 * recebe o dado do comando ANTERIOR. Por isso lemos 2 frames seguidos.
 */

#include "as5047.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "as5047";
static spi_device_handle_t s_spi;

// Comando de leitura do registro ANGLECOM (0x3FFF):
//   read-bit (bit14)=1 -> 0x7FFF ; paridade par (bit15) -> 0xFFFF.
#define CMD_ANGLECOM 0xFFFF

// paridade: 1 se o nº de bits 1 for impar
static inline int parity_odd(uint16_t v)
{
    v ^= v >> 8; v ^= v >> 4; v ^= v >> 2; v ^= v >> 1;
    return v & 1;
}

static uint16_t xfer16(uint16_t out)
{
    uint8_t tx[2] = { (uint8_t)(out >> 8), (uint8_t)(out & 0xFF) };
    uint8_t rx[2] = { 0, 0 };
    spi_transaction_t t = { .length = 16, .tx_buffer = tx, .rx_buffer = rx };
    spi_device_polling_transmit(s_spi, &t);
    return ((uint16_t)rx[0] << 8) | rx[1];
}

void as5047_init(void)
{
    spi_bus_config_t bus = {
        .sclk_io_num = AS5047_PIN_SCLK,
        .mosi_io_num = AS5047_PIN_MOSI,
        .miso_io_num = AS5047_PIN_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(AS5047_SPI_HOST, &bus, SPI_DMA_DISABLED));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = AS5047_SPI_HZ,
        .mode = 1,                      // CPOL=0, CPHA=1 (AS5047P)
        .spics_io_num = AS5047_PIN_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(AS5047_SPI_HOST, &dev, &s_spi));

    // Pull-up no MISO: sem sensor a linha fica em 0xFFFF (rejeitada por error-flag),
    // evitando leituras de pino flutuante. Com o sensor ligado, ele dirige a linha.
    gpio_set_pull_mode(AS5047_PIN_MISO, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "AS5047P SPI init (CLK=%d MISO=%d MOSI=%d CS=%d, mode1, %d Hz)",
             AS5047_PIN_SCLK, AS5047_PIN_MISO, AS5047_PIN_MOSI, AS5047_PIN_CS, AS5047_SPI_HZ);
}

int32_t as5047_read_raw(void)
{
    xfer16(CMD_ANGLECOM);                 // 1o frame: envia comando
    uint16_t r = xfer16(CMD_ANGLECOM);    // 2o frame: recebe o angulo

    // bit15 = paridade par sobre os bits 0-14: o frame inteiro deve ter nº PAR de 1s
    if (parity_odd(r)) return -1;
    // bit14 = error flag
    if (r & 0x4000) return -1;
    return r & 0x3FFF;                    // bits 13-0 = angulo
}

float as5047_read_deg(void)
{
    int32_t raw = as5047_read_raw();
    if (raw < 0) return -1.0f;
    return raw * 360.0f / AS5047_CPR;
}

uint16_t as5047_read_word(void)
{
    xfer16(CMD_ANGLECOM);                 // 1o frame: envia comando
    return xfer16(CMD_ANGLECOM);          // 2o frame: palavra crua (sem mascarar)
}
