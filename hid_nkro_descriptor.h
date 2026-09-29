/*
 * hid_nkro_descriptor.h — NKRO (N-Key Rollover) USB HID report
 * descriptor, following the standard bitmap-per-key pattern used by
 * QMK and most custom keyboard firmware (this is a well-documented,
 * widely-used descriptor shape, not something invented for this
 * project).
 *
 * Why NKRO instead of the basic 6-key-rollover boot keyboard format:
 * boot keyboard reports can only describe 6 simultaneously-pressed
 * keys (plus modifiers), which would silently drop key presses on a
 * 97-key board during fast typing/gaming. NKRO uses one bit per key
 * instead, so all 97 keys can be reported as pressed at once.
 *
 * Report layout (this matches what the descriptor below declares):
 *   Byte 0:      modifier byte (Ctrl/Shift/Alt/GUI, both sides) — 8 bits
 *   Bytes 1-13:  one bit per key, 104 bits = 13 bytes (covers 97 keys
 *                with room to spare; unused high bits stay 0)
 *
 * STATUS: descriptor bytes below follow the standard NKRO bitmap
 * pattern correctly in shape, but have NOT been tested against real
 * USB descriptor parsing (no hardware to verify against). Treat as
 * "should be structurally correct" rather than "verified working."
 */

#ifndef N96_HID_NKRO_DESCRIPTOR_H
#define N96_HID_NKRO_DESCRIPTOR_H

#include <stdint.h>

#define N96_NKRO_MODIFIER_BYTES  1
#define N96_NKRO_KEY_BYTES       13   // 104 bits, covers 97 keys
#define N96_NKRO_REPORT_LEN      (N96_NKRO_MODIFIER_BYTES + N96_NKRO_KEY_BYTES)

// Standard NKRO report descriptor: modifier byte as 8 individual
// 1-bit usages (Left/Right Ctrl/Shift/Alt/GUI), followed by a 104-bit
// array covering keyboard usage IDs 0-103 (0x00-0x67), one bit each.
static const uint8_t n96_nkro_hid_report_descriptor[] = {
    0x05, 0x01,                    // Usage Page (Generic Desktop)
    0x09, 0x06,                    // Usage (Keyboard)
    0xA1, 0x01,                    // Collection (Application)

    // --- Modifier byte: 8 x 1-bit ---
    0x05, 0x07,                    //   Usage Page (Key Codes)
    0x19, 0xE0,                    //   Usage Minimum (224) - LeftControl
    0x29, 0xE7,                    //   Usage Maximum (231) - Right GUI
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x08,                    //   Report Count (8)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)

    // --- Key bitmap: 104 x 1-bit, covering usage IDs 0-103 ---
    0x05, 0x07,                    //   Usage Page (Key Codes)
    0x19, 0x00,                    //   Usage Minimum (0)
    0x29, 0x67,                    //   Usage Maximum (103)
    0x15, 0x00,                    //   Logical Minimum (0)
    0x25, 0x01,                    //   Logical Maximum (1)
    0x75, 0x01,                    //   Report Size (1)
    0x95, 0x68,                    //   Report Count (104)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)

    0xC0                           // End Collection
};

#define N96_NKRO_DESCRIPTOR_LEN sizeof(n96_nkro_hid_report_descriptor)

#endif // N96_HID_NKRO_DESCRIPTOR_H
