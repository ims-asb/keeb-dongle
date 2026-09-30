#include "report.h"
#include "keepalive.h"
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

bool n96_report_any_held(const uint8_t nkro[N96_NKRO_REPORT_LEN])
{
    return n96_state_any_held(nkro[0], &nkro[N96_NKRO_MODIFIER_BYTES]);
}

void n96_boot_report_from_nkro(uint8_t boot[N96_BOOT_REPORT_LEN],
                               const uint8_t nkro[N96_NKRO_REPORT_LEN])
{
    const uint8_t *bm = &nkro[N96_NKRO_MODIFIER_BYTES];
    uint8_t keys[6];
    unsigned n = 0;
    bool overflow = false;

    for (unsigned usage = 4; usage <= N96_KEY_USAGE_MAX; usage++) {
        if (!n96_bitmask_get(bm, (uint8_t)usage)) continue;
        if (n == 6) { overflow = true; break; }
        keys[n++] = (uint8_t)usage;
    }
    boot[0] = nkro[0];
    boot[1] = 0;
    for (unsigned i = 0; i < 6; i++)
        boot[2 + i] = overflow ? N96_BOOT_ERR_ROLLOVER : (i < n ? keys[i] : 0);
}
