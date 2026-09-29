/*
 * Vendor-defined 16-byte input report used ONLY by the N96_TEST_MODE
 * firmware. Rationale: the NKRO keyboard interface cannot be safely used
 * for a rate test (a changing bitmap would type into the host, and
 * Windows does not let applications open keyboard collections for reading
 * via hidapi). A vendor-page device can be opened by tools/rate_test.py
 * on all three OSes and injects no keystrokes.
 *
 * Layout: bytes 0..3 = little-endian sequence counter, bytes 4..15 = a
 * fixed pattern derived from the counter (see report.c). Endpoint
 * settings (size 16, bInterval 1) are identical to the NKRO build.
 */
#ifndef N96_HID_TEST_DESCRIPTOR_H
#define N96_HID_TEST_DESCRIPTOR_H
#include <stdint.h>

#define N96_TEST_REPORT_LEN 16

static const uint8_t n96_test_hid_report_descriptor[] = {
    0x06, 0x00, 0xFF,              // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,                    // Usage (1)
    0xA1, 0x01,                    // Collection (Application)
    0x09, 0x02,                    //   Usage (2)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8)
    0x95, N96_TEST_REPORT_LEN,     //   Report Count (16)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)
    0xC0                           // End Collection
};

#define N96_TEST_DESCRIPTOR_LEN sizeof(n96_test_hid_report_descriptor)

#endif
