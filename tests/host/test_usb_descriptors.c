/*
 * Structural checks on the USB descriptors, run on the host by compiling
 * dongle/src/usb_descriptors.c against the real TinyUSB headers
 * (third_party/tinyusb, tag 0.19.0). Checks that the byte layout is
 * self-consistent and that the high-speed-only descriptors are present.
 * It does NOT check enumeration - that needs hardware.
 */
#ifndef N96_TEST_MODE
#define N96_TEST_MODE 0
#endif
#include "check.h"
#include "tusb.h"
#include "hid_nkro_descriptor.h"
#include "hid_test_descriptor.h"

extern uint8_t const *tud_descriptor_device_cb(void);
extern uint8_t const *tud_descriptor_device_qualifier_cb(void);
extern uint8_t const *tud_descriptor_configuration_cb(uint8_t);
extern uint8_t const *tud_descriptor_other_speed_configuration_cb(uint8_t);
extern uint16_t const *tud_descriptor_string_cb(uint8_t, uint16_t);
extern uint8_t const *tud_hid_descriptor_report_cb(uint8_t);

/* stubs for what board.c / TinyUSB core normally provide */
void board_uid_hex(char out[25]) { memcpy(out, "0123456789ABCDEF01234567", 25); }

static unsigned u16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }

static void test_device(void)
{
    const uint8_t *d = tud_descriptor_device_cb();
    CHECK_EQ(d[0], 18); CHECK_EQ(d[1], 1);
    CHECK_EQ(u16(d + 2), 0x0200);            /* USB 2.0: required for HS */
    CHECK_EQ(d[7], 64);                      /* bMaxPacketSize0: 64 is the only legal HS value */
    CHECK_EQ(d[17], 1);                      /* one configuration */
}

static void test_qualifier(void)
{
    const uint8_t *q = tud_descriptor_device_qualifier_cb();
    const uint8_t *d = tud_descriptor_device_cb();
    CHECK(q != NULL);
    CHECK_EQ(q[0], 10); CHECK_EQ(q[1], 6);   /* length 10, type DEVICE_QUALIFIER */
    CHECK_EQ(u16(q + 2), u16(d + 2));        /* bcdUSB */
    CHECK_EQ(q[4], d[4]); CHECK_EQ(q[5], d[5]); CHECK_EQ(q[6], d[6]);   /* class triple */
    CHECK_EQ(q[7], d[7]);                    /* EP0 size */
    CHECK_EQ(q[8], d[17]);                   /* num configurations */
    CHECK_EQ(q[9], 0);                       /* reserved */
}

/* Walk a configuration descriptor: every sub-descriptor length must chain to wTotalLength. */
static void check_config(const uint8_t *c, uint8_t expect_type)
{
    CHECK_EQ(c[0], 9); CHECK_EQ(c[1], expect_type);
    unsigned total = u16(c + 2);
    unsigned pos = 0, eps = 0, itfs = 0, hids = 0;
    while (pos < total) {
        unsigned len = c[pos];
        CHECK(len >= 2);
        if (len < 2) return;
        if (pos && c[pos + 1] == 0x04) itfs++;
        if (pos && c[pos + 1] == 0x21) hids++;
        if (pos && c[pos + 1] == 0x05) {
            eps++;
            CHECK_EQ(c[pos + 2], 0x81);              /* EP1 IN */
            CHECK_EQ(c[pos + 3], 0x03);              /* interrupt */
            CHECK_EQ(u16(c + pos + 4), 16);          /* wMaxPacketSize */
            CHECK_EQ(c[pos + 6], 1);                 /* bInterval 1: 125 us at HS, 1 ms at FS */
        }
        pos += len;
    }
    CHECK_EQ(pos, total);                            /* chain lands exactly on wTotalLength */
    CHECK_EQ(c[4], itfs);                            /* bNumInterfaces */
    CHECK_EQ(itfs, 1); CHECK_EQ(hids, 1); CHECK_EQ(eps, 1);
}

static void test_configs(void)
{
    const uint8_t *c = tud_descriptor_configuration_cb(0);
    const uint8_t *o = tud_descriptor_other_speed_configuration_cb(0);
    check_config(c, 2);                              /* CONFIGURATION */
    check_config(o, 7);                              /* OTHER_SPEED_CONFIGURATION */
    CHECK_EQ(u16(c + 2), u16(o + 2));
    CHECK(memcmp(c + 2, o + 2, u16(c + 2) - 2) == 0);   /* identical apart from the type byte */
    /* HID descriptor's report length matches the report descriptor we return */
    /* HID descriptor (offset 9 config + 9 interface): wDescriptorLength at +7 must equal the
     * report descriptor that tud_hid_descriptor_report_cb() returns. */
#if N96_TEST_MODE
    CHECK_EQ(u16(c + 18 + 7), N96_TEST_DESCRIPTOR_LEN);
    CHECK(memcmp(tud_hid_descriptor_report_cb(0), n96_test_hid_report_descriptor, N96_TEST_DESCRIPTOR_LEN) == 0);
#else
    CHECK_EQ(u16(c + 18 + 7), N96_NKRO_DESCRIPTOR_LEN);
    CHECK(memcmp(tud_hid_descriptor_report_cb(0), n96_nkro_hid_report_descriptor, N96_NKRO_DESCRIPTOR_LEN) == 0);
#endif
}

static void test_strings(void)
{
    const uint16_t *s0 = tud_descriptor_string_cb(0, 0);
    CHECK_EQ(s0[0], (3 << 8) | 4); CHECK_EQ(s0[1], 0x0409);
    for (uint8_t i = 1; i <= 3; i++) {
        const uint16_t *s = tud_descriptor_string_cb(i, 0x0409);
        CHECK(s != NULL);
        unsigned bytes = s[0] & 0xFF;
        CHECK_EQ(s[0] >> 8, 3);
        CHECK(bytes >= 4 && bytes % 2 == 0);
        for (unsigned k = 1; k < bytes / 2; k++) CHECK(s[k] >= 0x20 && s[k] < 0x7F);
    }
    const uint16_t *ser = tud_descriptor_string_cb(3, 0x0409);
    CHECK_EQ(ser[0] & 0xFF, 2 + 2 * 24);
    CHECK_EQ(ser[1], '0'); CHECK_EQ(ser[24], '7');
    CHECK(tud_descriptor_string_cb(4, 0x0409) == NULL);
}

int main(void)
{
    test_device(); test_qualifier(); test_configs(); test_strings();
    TEST_MAIN_END("test_usb_descriptors");
}
