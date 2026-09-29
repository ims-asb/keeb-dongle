#include "report.h"
#include <string.h>

_Static_assert(N96_NKRO_KEY_BYTES == N96_KEY_BITMASK_BYTES,
               "HID bitmap size must equal the packet bitmask size");

void n96_report_from_key_state(uint8_t report[N96_NKRO_REPORT_LEN],
                               const n96_key_state_packet_t *pkt)
{
    report[0] = pkt->modifiers;
    memcpy(&report[N96_NKRO_MODIFIER_BYTES], pkt->key_bitmask, N96_NKRO_KEY_BYTES);
}

bool n96_report_from_payload(uint8_t report[N96_NKRO_REPORT_LEN],
                             const uint8_t *payload, size_t len)
{
    n96_key_state_packet_t pkt;
    if (!n96_unpack_key_state(&pkt, payload, len)) return false;
    n96_report_from_key_state(report, &pkt);
    return true;
}

void n96_test_report(uint8_t report[N96_TEST_REPORT_LEN], uint32_t seq)
{
    report[0] = (uint8_t)(seq);
    report[1] = (uint8_t)(seq >> 8);
    report[2] = (uint8_t)(seq >> 16);
    report[3] = (uint8_t)(seq >> 24);
    for (unsigned i = 4; i < N96_TEST_REPORT_LEN; i++) report[i] = (uint8_t)(seq + i);
}
