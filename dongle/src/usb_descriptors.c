/*
 * TinyUSB descriptors: HID NKRO keyboard, HIGH-SPEED, bInterval = 1.
 * USB 2.0 spec: for high-speed interrupt endpoints the period is
 * 2^(bInterval-1) microframes of 125 us, so bInterval=1 -> 125 us -> 8 kHz.
 * The device must actually enumerate at High Speed (needs the ULPI PHY
 * working); at Full Speed the same descriptor would be capped near 1 kHz.
 *
 * TinyUSB macro/callback names below are from memory of its HID example -
 * verify against your vendored version. HS builds also need the
 * device-qualifier and other-speed-configuration callbacks.
 */
#include "tusb.h"
#include "hid_nkro_descriptor.h"

#define USB_VID 0xCAFE   /* TODO: placeholder - not a real assigned ID */
#define USB_PID 0x0096

static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID, .idProduct = USB_PID, .bcdDevice = 0x0100,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1,
};
uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&desc_device; }

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{ (void)instance; return n96_nkro_hid_report_descriptor; }

#define EPNUM_HID   0x81
#define HID_EP_SIZE 16          /* >= 14-byte report */
#define HID_BINTERVAL 1         /* HS: 125 us */
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

static uint8_t const desc_config[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_ITF_PROTOCOL_NONE, N96_NKRO_DESCRIPTOR_LEN,
                       EPNUM_HID, HID_EP_SIZE, HID_BINTERVAL),
};
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) { (void)index; return desc_config; }

/* TODO (HS required): tud_descriptor_device_qualifier_cb() and
 * tud_descriptor_other_speed_configuration_cb() - copy from a TinyUSB
 * HS example. String descriptors (tud_descriptor_string_cb) also TODO. */

uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t *b, uint16_t n)
{ (void)i;(void)id;(void)t;(void)b;(void)n; return 0; }
void tud_hid_set_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t const *b, uint16_t n)
{ (void)i;(void)id;(void)t;(void)b;(void)n; /* TODO: LED state (caps lock) */ }
