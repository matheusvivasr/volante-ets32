/*
 * as5600.c – driver I2C do sensor de angulo AS5600 (12 bits).
 *
 * Usa o driver NOVO de I2C master do IDF 5.x (driver/i2c_master.h).
 * Le RAW ANGLE (0x0C/0x0D) a cada chamada; checa o status do ima no init.
 */

#include "as5600.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "as5600";
static i2c_master_dev_handle_t s_dev = NULL;

// Le n bytes a partir do registrador reg. Retorna 0 em OK, -1 em erro.
static int rd(uint8_t reg, uint8_t *buf, size_t n)
{
    if (!s_dev) return -1;
    return (i2c_master_transmit_receive(s_dev, &reg, 1, buf, n, 50) == ESP_OK) ? 0 : -1;
}

void as5600_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = AS5600_SCL_PIN,
        .sda_io_num = AS5600_SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AS5600_ADDR,
        .scl_speed_hz = 400000,   // 400 kHz (fast mode)
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &s_dev));

    uint8_t st = 0;
    if (rd(0x0B, &st, 1) == 0) {
        bool md = st & 0x20, ml = st & 0x10, mh = st & 0x08;
        ESP_LOGI(TAG, "AS5600 init (SDA=%d SCL=%d, addr 0x%02X). Ima: %s%s%s",
                 AS5600_SDA_PIN, AS5600_SCL_PIN, AS5600_ADDR,
                 md ? "DETECTADO" : "AUSENTE",
                 ml ? " [fraco: aproxime]" : "",
                 mh ? " [forte: afaste]" : "");
    } else {
        ESP_LOGW(TAG, "AS5600 nao respondeu no I2C (confira SDA=%d SCL=%d, VCC=3V3, addr 0x36).",
                 AS5600_SDA_PIN, AS5600_SCL_PIN);
    }
}

int32_t as5600_read_raw(void)
{
    uint8_t b[2];
    if (rd(0x0C, b, 2) != 0) return -1;             // RAW ANGLE: 0x0C(hi) 0x0D(lo)
    return ((int32_t)(b[0] & 0x0F) << 8) | b[1];    // 12 bits -> 0..4095
}

float as5600_read_deg(void)
{
    int32_t raw = as5600_read_raw();
    if (raw < 0) return -1.0f;
    return raw * 360.0f / AS5600_CPR;
}
