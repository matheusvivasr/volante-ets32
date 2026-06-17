#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "transport.h"
#include "telemetry.h"
#include "as5600.h"
#include "usb_hid.h"
#include "botoeira_rx.h"
#include "calib.h"
#include "pedais.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "main_s3";

// ─── UART0 = USB serial do PC (recebe telemetria do leitor Python) ────────────
#define PC_UART_NUM  UART_NUM_0
#define PC_UART_BAUD 115200
#define PC_BUF_SIZE  512

// ─── Callback de mensagens vindas dos módulos satélites ───────────────────────
static void on_module_msg(const transport_msg_t *msg)
{
    // Mensagens dos satélites que chegam pelo transporte (CAN). A botoeira
    // (0x040) agora vem pelo barramento; alimenta o mesmo estado que o hid_task
    // lê via botoeira_get(). (Na UART a botoeira chegava pela UART2/botoeira_rx.)
    switch (msg->id) {
        case MSG_ID_BOTOEIRA:
            botoeira_rx_feed(msg->payload, msg->len);
            break;
        default:
            ESP_LOGD(TAG, "msg id=0x%03X len=%d", msg->id, msg->len);
            break;
    }
}

// ─── Task: recebe telemetria do PC e atualiza a imagem local ─────────────────
static void pc_rx_task(void *arg)
{
    // Reutiliza o mesmo framing do transport.h, só que na UART do PC
    // (USB serial = UART0 no ESP32-S3 DevKit)
    uint8_t buf[PC_BUF_SIZE];

    while (1) {
        uint8_t byte;
        int r = uart_read_bytes(PC_UART_NUM, &byte, 1, pdMS_TO_TICKS(100));
        if (r <= 0 || byte != 0xAA) continue; // aguarda SOF

        uint8_t hdr[3];
        r = uart_read_bytes(PC_UART_NUM, hdr, 3, pdMS_TO_TICKS(20));
        if (r < 3) continue;

        uint16_t id  = hdr[0] | ((uint16_t)hdr[1] << 8);
        uint8_t  len = hdr[2];
        if (len > 8) continue;

        uint8_t tail[10]; // payload(8) + seq(1) + crc(1)
        r = uart_read_bytes(PC_UART_NUM, tail, len + 2, pdMS_TO_TICKS(20));
        if (r < len + 2) continue;

        uint8_t seq = tail[len];
        // TODO: validar CRC aqui (mesma função crc8 do transport.c)

        switch (id) {
            case MSG_ID_TELEM_PAINEL:
                telemetry_update_painel(tail, len, seq);
                break;
            case MSG_ID_TELEM_LEDS:
                telemetry_update_leds(tail, len, seq);
                break;
            case MSG_ID_TELEM_NAV:
                telemetry_update_nav(tail, len, seq);
                break;
            default:
                break;
        }
    }
    (void)buf;
}

// Último ângulo lido pelo hid_task (cache p/ não disputar o SPI em 2 tasks).
static volatile int32_t s_angle_raw = -1;

// ─── Task: lê o AS5600 (I2C), atualiza o gamepad HID e cacheia o ângulo (50 Hz) ─
// X = direção (calibrada pelo BOOT); Y/Rx = acelerador/freio (pedais);
// Z/Rz = visão (joystick da Botoeira); botões 1-8 = Botoeira, 16 = BOOT (teste).
static void hid_task(void *arg)
{
    while (1) {
        int32_t raw = as5600_read_raw();
        s_angle_raw = raw;

        int8_t x = calib_steering(raw);   // multi-turn + mapa calibrado (0 sem sensor)
        calib_button_poll();              // roteiro de calibracao pelo BOOT

        int8_t accel, brake;
        pedais_get(&accel, &brake);       // acelerador/freio (botoes ou ADC)

        // Botões e joystick de visão vindos da Botoeira (0x040 via UART2)
        uint16_t bot_btn; int8_t vx, vy;
        botoeira_get(&bot_btn, &vx, &vy);
        uint32_t buttons = bot_btn;                       // bits 0-7 = botoeira
        if (calib_hid_button()) buttons |= 0x8000;        // BOOT (so fora da calibracao) = botão 16

        usb_hid_send(x, accel, brake, vx, vy, buttons);

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─── Task: diagnostico do AS5600 a 1 Hz (bring-up; loga o angulo cacheado) ────
static void angle_task(void *arg)
{
    while (1) {
        int32_t raw = s_angle_raw;
        if (raw < 0)
            ESP_LOGW("angle", "sem leitura valida (confira I2C SDA=6/SCL=7, VCC=3V3, ima).");
        else
            ESP_LOGI("angle", "raw=%5d  ang=%6.2f deg", (int)raw, raw * 360.0f / AS5600_CPR);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ─── Task: reemite telemetria a cada 50 ms para o painel C3 ──────────────────
static void reemit_task(void *arg)
{
    while (1) {
        telemetry_reemit();
        telemetry_tick();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ─── app_main ─────────────────────────────────────────────────────────────────
void app_main(void)
{
    ESP_LOGI(TAG, "=== Volante DIY – ESP32-S3 Host ===");
    ESP_LOGI(TAG, "Etapa 2: volante HID (direcao + pedais + botoeira + painel)");

    // NVS: armazena a calibracao da direcao (sobrevive ao reset)
    esp_err_t nvs = nvs_flash_init();
    if (nvs == ESP_ERR_NVS_NO_FREE_PAGES || nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Init do transporte UART para o painel C3
    transport_init(on_module_msg);

    // Init da imagem de telemetria
    telemetry_init();

    // UART0 = USB serial do PC: PRECISA instalar o driver, senão uart_read_bytes
    // falha na hora, o pc_rx_task gira em vazio (prio 6) e mata de fome o reemit (prio 4).
    uart_config_t pc_cfg = {
        .baud_rate = PC_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(PC_UART_NUM, &pc_cfg));
    ESP_ERROR_CHECK(uart_driver_install(PC_UART_NUM, PC_BUF_SIZE, 0, 0, NULL, 0));

    // Task de recepção do PC (USB serial / UART0)
    xTaskCreate(pc_rx_task,   "pc_rx",   4096, NULL, 6, NULL);

    // Task de reemissão para o painel C3
    xTaskCreate(reemit_task,  "reemit",  2048, NULL, 4, NULL);

    // Sensor de ângulo AS5600 (I2C: SDA=GPIO6, SCL=GPIO7)
    as5600_init();

    // Calibracao da direcao + botao BOOT (configura o GPIO0 internamente)
    calib_init();

    // Acelerador/freio (botoes hoje, potenciometros no futuro — ver pedais.h)
    pedais_init();

    // Recepção da Botoeira (UART2, msg 0x040)
    botoeira_rx_init();

    // USB HID gamepad na USB nativa (etapa 2)
    usb_hid_init();

    // Task que lê o sensor + alimenta o HID (50 Hz) e task que loga o ângulo (1 Hz)
    xTaskCreate(hid_task,     "hid",     4096, NULL, 5, NULL);
    xTaskCreate(angle_task,   "angle",   3072, NULL, 4, NULL);

    ESP_LOGI(TAG, "Tasks iniciadas. Aguardando telemetria do PC...");
}
