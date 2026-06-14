/*
 * usb_hid.c – gamepad USB HID na USB nativa da S3 (TinyUSB / esp_tinyusb).
 *
 * Usa o report de gamepad padrao do TinyUSB (x,y,z,rz,rx,ry,hat,buttons).
 * O eixo X carrega a direcao. CFG_TUD_HID=1 vem do CONFIG_TINYUSB_HID_COUNT.
 */

#include "usb_hid.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "esp_log.h"

static const char *TAG = "usb_hid";

// ── HID report descriptor: gamepad padrao (sem report ID) ──
static const uint8_t s_hid_report_desc[] = {
    TUD_HID_REPORT_DESC_GAMEPAD()
};

// ── String descriptors ──
static const char *s_str_desc[] = {
    (char[]){0x09, 0x04},   // 0: LANGID en-US
    "Volante DIY",           // 1: fabricante
    "Volante S3 Wheel",      // 2: produto
    "0001",                  // 3: serial
    "Volante HID",           // 4: interface HID
};

// ── Device descriptor ──
static const tusb_desc_device_t s_dev_desc = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A,   // Espressif
    .idProduct          = 0x4004,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01,
};

// ── Configuration descriptor (1 interface HID, EP IN 0x81) ──
#define EPNUM_HID    0x81
#define HID_STR_IDX  4
static const uint8_t s_cfg_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN),
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, HID_STR_IDX, false, sizeof(s_hid_report_desc),
                       EPNUM_HID, 16, 10),
};

// ── Callbacks obrigatorios do TinyUSB HID ──
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return s_hid_report_desc;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                           hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

void usb_hid_init(void)
{
    const tinyusb_config_t cfg = {
        .device_descriptor        = &s_dev_desc,
        .string_descriptor        = s_str_desc,
        .string_descriptor_count  = sizeof(s_str_desc) / sizeof(s_str_desc[0]),
        .external_phy             = false,
        .configuration_descriptor = s_cfg_desc,
        .self_powered             = false,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&cfg));
    ESP_LOGI(TAG, "USB HID gamepad instalado (plugue a USB NATIVA p/ aparecer no PC)");
}

bool usb_hid_ready(void)
{
    return tud_mounted();
}

void usb_hid_send(int8_t steering_x, int8_t throttle_y, int8_t brake_rx,
                  int8_t view_x, int8_t view_y, uint32_t buttons)
{
    if (!tud_mounted()) return;
    // gamepad(report_id, x, y, z, rz, rx, ry, hat, buttons):
    //   X = direcao; Y = acelerador; Rx = freio; Z = visao horiz; Rz = visao vert.
    tud_hid_gamepad_report(0, steering_x, throttle_y, view_x, view_y, brake_rx, 0, 0, buttons);
}
