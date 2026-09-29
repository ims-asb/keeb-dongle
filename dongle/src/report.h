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

#endif
