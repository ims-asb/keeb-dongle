/* Pure functions that build USB HID reports. No hardware dependencies, so
 * they are unit-tested on the host (tests/host). */
#ifndef N96_REPORT_H
#define N96_REPORT_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "protocol.h"
#include "hid_nkro_descriptor.h"
#include "hid_test_descriptor.h"

/* Build the 14-byte NKRO report from a key-state packet. */
void n96_report_from_key_state(uint8_t report[N96_NKRO_REPORT_LEN],
                               const n96_key_state_packet_t *pkt);

/* Parse a raw radio payload and, if it is a valid key-state packet, fill
 * `report` and return true. Other/short/long payloads leave `report`
 * untouched and return false. */
bool n96_report_from_payload(uint8_t report[N96_NKRO_REPORT_LEN],
                             const uint8_t *payload, size_t len);

/* Test-mode report for sequence number `seq`: bytes 0..3 seq (LE),
 * bytes 4..15 = (uint8_t)(seq + i), so every report differs. */
void n96_test_report(uint8_t report[N96_TEST_REPORT_LEN], uint32_t seq);


/* ---- HID boot protocol ------------------------------------------------- */
#define N96_BOOT_REPORT_LEN 8    /* modifiers, reserved, 6 keycodes (HID boot keyboard) */
#define N96_BOOT_ERR_ROLLOVER 0x01   /* Keyboard ErrorRollOver usage */

/* True if any modifier or key bit is set in an NKRO report. */
bool n96_report_any_held(const uint8_t nkro[N96_NKRO_REPORT_LEN]);

/* Convert the NKRO report to the 8-byte boot-protocol report. Keys are
 * emitted in ascending usage order (press order is not kept). Usages 0-3
 * (reserved / error codes) are never emitted from the bitmap. With more than
 * six keys down the six keycodes are all ErrorRollOver, as the boot protocol
 * requires. Modifiers are always reported. */
void n96_boot_report_from_nkro(uint8_t boot[N96_BOOT_REPORT_LEN],
                               const uint8_t nkro[N96_NKRO_REPORT_LEN]);

#endif
