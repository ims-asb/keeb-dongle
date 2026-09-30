/*
 * TinyUSB descriptors: HID NKRO keyboard, HIGH-SPEED capable, bInterval = 1.
 * USB 2.0: for high-speed interrupt endpoints the period is 2^(bInterval-1)
 * microframes of 125 us, so bInterval=1 -> 125 us. That is the polling
 * INTERVAL the device asks for; it is not a throughput or latency claim.
 * The device must actually enumerate at High Speed (ULPI PHY working); at
 * Full Speed the same byte means 1 ms.
 *
 * A high-speed capable device must answer GET_DESCRIPTOR for the device
 * qualifier and other-speed configuration (USB 2.0 ch. 9); TinyUSB's weak
 * defaults return NULL, which stalls the request (VERIFY.md section 3).
 *
 * Build with -DN96_TEST_MODE=1 for the rate-test firmware: same endpoint
 * configuration, but a vendor-defined report and a different PID/product
 * string so it never types into the host.
 */
#include <string.h>
#include "tusb.h"
#include "board.h"
#include "hid_nkro_descriptor.h"
#include "hid_test_descriptor.h"

#ifndef N96_TEST_MODE
#define N96_TEST_MODE 0
#endif

#define USB_VID 0xCAFE   /* TODO: placeholder - not a real assigned ID */
#if N96_TEST_MODE
#define USB_PID 0x0097   /* test firmware: distinct so OS caches don't mix them */
#else
#define USB_PID 0x0096
#endif
#define USB_BCD 0x0200

#if N96_TEST_MODE
#define REPORT_DESC      n96_test_hid_report_descriptor
#define REPORT_DESC_LEN  N96_TEST_DESCRIPTOR_LEN
#define HID_PROTOCOL     HID_ITF_PROTOCOL_NONE
#else
#define REPORT_DESC      n96_nkro_hid_report_descriptor
#define REPORT_DESC_LEN  N96_NKRO_DESCRIPTOR_LEN
#define HID_PROTOCOL     HID_ITF_PROTOCOL_KEYBOARD   /* boot-capable: subclass 1 / protocol 1. The host picks boot
                                                      * or report protocol with SET_PROTOCOL; see core.c */
#endif

static tusb_desc_device_t const desc_device = {
    .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = USB_BCD, .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID, .idProduct = USB_PID, .bcdDevice = 0x0100,
    .iManufacturer = 1, .iProduct = 2, .iSerialNumber = 3, .bNumConfigurations = 1,
};
uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&desc_device; }

/* Device qualifier: describes the device as it would look at the *other*
 * speed. Same class/EP0 size/config count as the device descriptor. */
static tusb_desc_device_qualifier_t const desc_device_qualifier = {
    .bLength = sizeof(tusb_desc_device_qualifier_t),
    .bDescriptorType = TUSB_DESC_DEVICE_QUALIFIER,
    .bcdUSB = USB_BCD,
    .bDeviceClass = 0, .bDeviceSubClass = 0, .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .bNumConfigurations = 1,
    .bReserved = 0,
};
uint8_t const *tud_descriptor_device_qualifier_cb(void) { return (uint8_t const *)&desc_device_qualifier; }

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{ (void)instance; return REPORT_DESC; }

#define EPNUM_HID   0x81
#define HID_EP_SIZE 16          /* >= 14-byte NKRO / 16-byte test report; also CFG_TUD_HID_EP_BUFSIZE */
#define HID_BINTERVAL 1         /* HS: 125 us; FS: 1 ms - the same byte is valid at both speeds */
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)

_Static_assert(HID_EP_SIZE <= CFG_TUD_HID_EP_BUFSIZE, "HID EP buffer too small");
_Static_assert(N96_NKRO_REPORT_LEN <= HID_EP_SIZE, "NKRO report does not fit in the endpoint");
_Static_assert(N96_TEST_REPORT_LEN <= HID_EP_SIZE, "test report does not fit in the endpoint");

/* One configuration serves both speeds: the endpoint size (16) is legal at
 * both and bInterval=1 is valid at both, so no per-speed variant is needed. */
static uint8_t const desc_config[CONFIG_TOTAL_LEN] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 0, HID_PROTOCOL, REPORT_DESC_LEN,
                       EPNUM_HID, HID_EP_SIZE, HID_BINTERVAL),
};
uint8_t const *tud_descriptor_configuration_cb(uint8_t index) { (void)index; return desc_config; }

/* Other-speed configuration: identical to the configuration but with
 * bDescriptorType = OTHER_SPEED_CONFIGURATION. Kept in RAM because the
 * type byte differs; the buffer must outlive the control transfer. */
uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
    static uint8_t desc_other_speed[CONFIG_TOTAL_LEN];
    (void)index;
    memcpy(desc_other_speed, desc_config, CONFIG_TOTAL_LEN);
    desc_other_speed[1] = TUSB_DESC_OTHER_SPEED_CONFIG;
    return desc_other_speed;
}

/* ---- Strings ---------------------------------------------------------- */
enum { STR_LANGID = 0, STR_MANUF, STR_PRODUCT, STR_SERIAL };

static char const *const string_desc_arr[] = {
    [STR_LANGID]  = (const char[]){ 0x09, 0x04 },   /* 0x0409 English (US) */
    [STR_MANUF]   = "Nano96 (prototype)",
#if N96_TEST_MODE
    [STR_PRODUCT] = "Nano96 Dongle RATE TEST",
#else
    [STR_PRODUCT] = "Nano96 Dongle",
#endif
    [STR_SERIAL]  = NULL,                           /* filled from the MCU unique ID */
};

/* UTF-16 buffer: 1 header word + up to 31 characters. */
static uint16_t desc_str[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    uint8_t chr_count;
    char serial[25];

    if (index == STR_LANGID) {
        memcpy(&desc_str[1], string_desc_arr[STR_LANGID], 2);
        chr_count = 1;
    } else {
        if (index >= sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) return NULL;
        const char *str = string_desc_arr[index];
        if (index == STR_SERIAL) { board_uid_hex(serial); str = serial; }

        chr_count = (uint8_t)strlen(str);
        if (chr_count > 31) chr_count = 31;
        for (uint8_t i = 0; i < chr_count; i++) desc_str[1 + i] = (uint16_t)(uint8_t)str[i];
    }
    /* first word: length in bytes (incl. header) in the low byte, type STRING in the high byte */
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return desc_str;
}
