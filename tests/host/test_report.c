#include "check.h"
#include "report.h"

/* Minimal HID report-descriptor walker: returns total Input bits, or -1 if
 * the descriptor is malformed (bad item sizes, unbalanced collections, no
 * Report Size/Count before an Input item). Only what our two descriptors use. */
static int hid_input_bits(const uint8_t *d, size_t n)
{
    int size = -1, count = -1, depth = 0, bits = 0;
    size_t i = 0;
    while (i < n) {
        uint8_t b = d[i];
        unsigned sz = b & 3u; if (sz == 3) sz = 4;
        uint8_t tag = b & 0xFC;
        if (b == 0xFE) return -1;                        /* long items unused */
        if (i + 1 + sz > n) return -1;
        unsigned v = 0;
        for (unsigned k = 0; k < sz; k++) v |= (unsigned)d[i + 1 + k] << (8 * k);
        switch (tag) {
        case 0x74: size = (int)v; break;                 /* Report Size */
        case 0x94: count = (int)v; break;                /* Report Count */
        case 0xA0: depth++; break;                       /* Collection */
        case 0xC0: depth--; if (depth < 0) return -1; break;
        case 0x80:                                       /* Input */
            if (size < 0 || count < 0) return -1;
            bits += size * count; break;
        default: break;
        }
        i += 1 + sz;
    }
    return depth == 0 ? bits : -1;
}

static void test_descriptors(void)
{
    CHECK_EQ(hid_input_bits(n96_nkro_hid_report_descriptor, N96_NKRO_DESCRIPTOR_LEN), N96_NKRO_REPORT_LEN * 8);
    CHECK_EQ(N96_NKRO_REPORT_LEN, 14);
    CHECK_EQ(hid_input_bits(n96_test_hid_report_descriptor, N96_TEST_DESCRIPTOR_LEN), N96_TEST_REPORT_LEN * 8);
    CHECK_EQ(N96_TEST_REPORT_LEN, 16);
    /* NKRO: last usage in the bitmap is 0x67 (103) and matches the packet's range */
    CHECK_EQ(n96_nkro_hid_report_descriptor[26], 0x29);
    CHECK_EQ(n96_nkro_hid_report_descriptor[27], N96_KEY_USAGE_MAX);
}

static void test_from_key_state(void)
{
    uint8_t bm[N96_KEY_BITMASK_BYTES] = {0};
    n96_bitmask_set(bm, 0x04, true);   /* a */
    n96_bitmask_set(bm, 0x1D, true);   /* z */
    n96_bitmask_set(bm, 0x67, true);
    n96_key_state_packet_t p; n96_pack_key_state(&p, 7, 0x22, bm);   /* LShift + RShift-bit */
    uint8_t r[N96_NKRO_REPORT_LEN]; memset(r, 0xEE, sizeof r);
    n96_report_from_key_state(r, &p);
    CHECK_EQ(r[0], 0x22);
    CHECK(memcmp(&r[1], bm, 13) == 0);
    /* HID bit position: bitmap bit N is usage N, i.e. report byte 1 + N/8, bit N%8 */
    CHECK(r[1 + 0] & (1u << 4));       /* usage 4 */
    CHECK(r[1 + 3] & (1u << 5));       /* usage 29 = 3*8+5 */
    CHECK(r[1 + 12] & (1u << 7));      /* usage 103 */
}

static void test_from_payload(void)
{
    uint8_t bm[N96_KEY_BITMASK_BYTES] = {0};
    n96_bitmask_set(bm, 0x2C, true);   /* space */
    n96_key_state_packet_t p; n96_pack_key_state(&p, 1, 0x01, bm);

    uint8_t r[N96_NKRO_REPORT_LEN]; memset(r, 0x5A, sizeof r);
    CHECK(n96_report_from_payload(r, (const uint8_t *)&p, sizeof p));
    CHECK_EQ(r[0], 0x01);
    CHECK(n96_bitmask_get(&r[1], 0x2C));

    uint8_t keep[N96_NKRO_REPORT_LEN]; memset(r, 0x5A, sizeof r); memcpy(keep, r, sizeof r);
    CHECK(!n96_report_from_payload(r, (const uint8_t *)&p, sizeof p - 1));
    uint8_t hb[16] = { N96_PKT_HEARTBEAT };
    CHECK(!n96_report_from_payload(r, hb, sizeof hb));
    CHECK(memcmp(r, keep, sizeof r) == 0);      /* rejected payloads leave the report untouched */

    /* all keys up must produce an all-zero report (clears stuck keys) */
    uint8_t zero[N96_KEY_BITMASK_BYTES] = {0};
    n96_pack_key_state(&p, 2, 0, zero);
    CHECK(n96_report_from_payload(r, (const uint8_t *)&p, sizeof p));
    for (int i = 0; i < N96_NKRO_REPORT_LEN; i++) CHECK_EQ(r[i], 0);
}

static void test_test_report(void)
{
    uint8_t a[N96_TEST_REPORT_LEN], b[N96_TEST_REPORT_LEN];
    n96_test_report(a, 0x01020304u);
    CHECK_EQ(a[0], 0x04); CHECK_EQ(a[1], 0x03); CHECK_EQ(a[2], 0x02); CHECK_EQ(a[3], 0x01);
    n96_test_report(b, 0x01020305u);
    CHECK(memcmp(a, b, sizeof a) != 0);          /* every report differs from the last */
    /* consecutive sequence numbers never give identical reports, including across the wrap */
    for (uint32_t s = 0xFFFFFF00u; s != 0x00000100u; s++) {
        n96_test_report(a, s); n96_test_report(b, s + 1);
        if (memcmp(a, b, sizeof a) == 0) { CHECK(0); break; }
    }
    CHECK(1);
}

static void nkro_with(uint8_t r[N96_NKRO_REPORT_LEN], uint8_t mods, const uint8_t *usages, int n)
{
    memset(r, 0, N96_NKRO_REPORT_LEN);
    r[0] = mods;
    for (int i = 0; i < n; i++) n96_bitmask_set(&r[1], usages[i], true);
}

static void test_boot_report(void)
{
    uint8_t nk[N96_NKRO_REPORT_LEN], b[N96_BOOT_REPORT_LEN];
    CHECK_EQ(N96_BOOT_REPORT_LEN, 8);

    nkro_with(nk, 0, NULL, 0);                      /* idle */
    memset(b, 0xEE, sizeof b); n96_boot_report_from_nkro(b, nk);
    for (int i = 0; i < 8; i++) CHECK_EQ(b[i], 0);
    CHECK(!n96_report_any_held(nk));

    const uint8_t one[] = { 0x04 };
    nkro_with(nk, 0x22, one, 1);                    /* modifiers + 'a' */
    n96_boot_report_from_nkro(b, nk);
    CHECK_EQ(b[0], 0x22); CHECK_EQ(b[1], 0); CHECK_EQ(b[2], 0x04);
    for (int i = 3; i < 8; i++) CHECK_EQ(b[i], 0);
    CHECK(n96_report_any_held(nk));

    const uint8_t six[] = { 0x2C, 0x04, 0x1D, 0x28, 0x05, 0x67 };
    nkro_with(nk, 0, six, 6);                       /* exactly six: ascending order, no rollover */
    n96_boot_report_from_nkro(b, nk);
    const uint8_t want6[] = { 0x04, 0x05, 0x1D, 0x28, 0x2C, 0x67 };
    for (int i = 0; i < 6; i++) CHECK_EQ(b[2 + i], want6[i]);

    const uint8_t seven[] = { 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
    nkro_with(nk, 0x01, seven, 7);                  /* seven: all ErrorRollOver, modifiers kept */
    n96_boot_report_from_nkro(b, nk);
    CHECK_EQ(b[0], 0x01); CHECK_EQ(b[1], 0);
    for (int i = 0; i < 6; i++) CHECK_EQ(b[2 + i], N96_BOOT_ERR_ROLLOVER);

    const uint8_t low[] = { 0, 1, 2, 3 };           /* reserved/error usages are never emitted */
    nkro_with(nk, 0, low, 4);
    n96_boot_report_from_nkro(b, nk);
    for (int i = 2; i < 8; i++) CHECK_EQ(b[i], 0);

    nkro_with(nk, 0x80, NULL, 0);                   /* modifier-only */
    n96_boot_report_from_nkro(b, nk);
    CHECK_EQ(b[0], 0x80); CHECK(n96_report_any_held(nk));
}

int main(void)
{
    test_descriptors(); test_boot_report(); test_from_key_state(); test_from_payload(); test_test_report();
    TEST_MAIN_END("test_report");
}
