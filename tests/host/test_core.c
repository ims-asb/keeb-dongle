/* End-to-end host tests of dongle/src/core.c: radio payloads, hold timeout, boot/report protocol. */
#include "check.h"
#include "core.h"
#include "keepalive.h"

static size_t pkt(uint8_t out[16], uint8_t seq, uint8_t mods, uint8_t usage)
{
    uint8_t bm[N96_KEY_BITMASK_BYTES] = {0};
    if (usage) n96_bitmask_set(bm, usage, true);
    n96_key_state_packet_t p;
    size_t n = n96_pack_key_state(&p, seq, mods, bm);
    memcpy(out, &p, n);
    return n;
}

static void test_payload_updates_state(void)
{
    n96_core_t c; n96_core_init(&c);
    uint8_t raw[16]; size_t n = pkt(raw, 1, 0x02, 0x04);
    CHECK(n96_core_on_payload(&c, raw, n, 1000));
    CHECK(c.dirty);
    uint8_t out[N96_NKRO_REPORT_LEN];
    CHECK_EQ(n96_core_report(&c, out), 14);
    CHECK_EQ(out[0], 0x02);
    CHECK(n96_bitmask_get(&out[1], 0x04));
    c.dirty = false;
    uint8_t junk[16] = { N96_PKT_HEARTBEAT };
    CHECK(!n96_core_on_payload(&c, junk, 16, 1001));
    CHECK(!c.dirty);                                /* rejected payload changes nothing */
    CHECK(!n96_core_on_payload(&c, raw, 15, 1002));
}

static void test_seq_gap_counting(void)
{
    n96_core_t c; n96_core_init(&c);
    uint8_t raw[16];
    pkt(raw, 10, 0, 0x04); n96_core_on_payload(&c, raw, 16, 0);
    pkt(raw, 11, 0, 0x04); n96_core_on_payload(&c, raw, 16, 20);
    pkt(raw, 14, 0, 0x04); n96_core_on_payload(&c, raw, 16, 40);
    CHECK_EQ(c.rf_missed, 2);
}

static void test_timeout_releases_held_key(void)
{
    n96_core_t c; n96_core_init(&c);
    uint8_t raw[16]; pkt(raw, 1, 0, 0x04);
    n96_core_on_payload(&c, raw, 16, 5000);
    c.dirty = false;
    CHECK(!n96_core_tick(&c, 5000 + N96_HOLD_TIMEOUT_MS - 1));
    CHECK(!c.dirty);
    CHECK(n96_core_tick(&c, 5000 + N96_HOLD_TIMEOUT_MS));
    CHECK(c.dirty);                                 /* a release must be sent to the host */
    CHECK_EQ(c.timeouts, 1);
    uint8_t out[N96_NKRO_REPORT_LEN];
    n96_core_report(&c, out);
    for (int i = 0; i < 14; i++) CHECK_EQ(out[i], 0);
    /* once released it does not fire again */
    c.dirty = false;
    CHECK(!n96_core_tick(&c, 5000 + 10 * N96_HOLD_TIMEOUT_MS));
    CHECK(!c.dirty);
}

static void test_resends_keep_key_alive(void)
{
    /* keyboard resends every N96_RESEND_INTERVAL_MS; dongle must never time out while they arrive */
    n96_core_t c; n96_core_init(&c);
    n96_resend_t kb; n96_resend_init(&kb);
    uint8_t bm[N96_KEY_BITMASK_BYTES] = {0}; n96_bitmask_set(bm, 0x04, true);
    uint8_t raw[16]; uint8_t seq = 0;
    uint32_t now = 100000;
    n96_key_state_packet_t p; n96_pack_key_state(&p, seq++, 0, bm); memcpy(raw, &p, 16);
    n96_core_on_payload(&c, raw, 16, now);
    n96_resend_note_sent(&kb, 0, bm, now);
    for (int ms = 0; ms < 5000; ms++, now++) {
        if (n96_resend_due(&kb, now)) {
            n96_pack_key_state(&p, seq++, kb.modifiers, kb.bitmask); memcpy(raw, &p, 16);
            n96_core_on_payload(&c, raw, 16, now);
            n96_resend_note_sent(&kb, kb.modifiers, kb.bitmask, now);
        }
        CHECK(!n96_core_tick(&c, now));
    }
    CHECK_EQ(c.timeouts, 0);
    CHECK_EQ(c.rf_missed, 0);
    CHECK(n96_report_any_held(c.nkro));
}

static void test_lost_release_recovered_two_ways(void)
{
    /* (a) keyboard side: release TX failed -> retry gets it through */
    n96_core_t c; n96_core_init(&c);
    n96_resend_t kb; n96_resend_init(&kb);
    uint8_t bm[N96_KEY_BITMASK_BYTES] = {0}; n96_bitmask_set(bm, 0x04, true);
    uint8_t raw[16]; n96_key_state_packet_t p;
    n96_pack_key_state(&p, 0, 0, bm); memcpy(raw, &p, 16);
    n96_core_on_payload(&c, raw, 16, 0);
    memset(bm, 0, sizeof bm);
    n96_resend_note_sent(&kb, 0, bm, 50);          /* release "sent" at t=50 ... */
    n96_resend_tx_failed(&kb);                     /* ... but lost */
    CHECK(!n96_resend_due(&kb, 60));
    CHECK(n96_resend_due(&kb, 50 + N96_RESEND_INTERVAL_MS));
    n96_pack_key_state(&p, 1, kb.modifiers, kb.bitmask); memcpy(raw, &p, 16);
    n96_core_on_payload(&c, raw, 16, 70);          /* retry arrives */
    CHECK(!n96_report_any_held(c.nkro));
    /* (b) keyboard vanished entirely: dongle timeout releases */
    n96_core_t d; n96_core_init(&d);
    uint8_t bm2[N96_KEY_BITMASK_BYTES] = {0}; n96_bitmask_set(bm2, 0x05, true);
    n96_pack_key_state(&p, 0, 0, bm2); memcpy(raw, &p, 16);
    n96_core_on_payload(&d, raw, 16, 0);
    CHECK(n96_core_tick(&d, N96_HOLD_TIMEOUT_MS));
    CHECK(!n96_report_any_held(d.nkro));
}

static void test_protocol_switch(void)
{
    n96_core_t c; n96_core_init(&c);
    CHECK_EQ(c.protocol, N96_PROTO_REPORT);        /* USB default is report protocol */
    uint8_t raw[16]; pkt(raw, 1, 0x01, 0x04);
    n96_core_on_payload(&c, raw, 16, 0);
    c.dirty = false;
    uint8_t out[N96_NKRO_REPORT_LEN];

    n96_core_set_protocol(&c, N96_PROTO_BOOT);
    CHECK(c.dirty);                                /* state is re-sent in the new format */
    CHECK_EQ(n96_core_report(&c, out), 8);
    CHECK_EQ(out[0], 0x01); CHECK_EQ(out[1], 0); CHECK_EQ(out[2], 0x04);

    n96_core_set_protocol(&c, N96_PROTO_REPORT);
    CHECK_EQ(n96_core_report(&c, out), 14);
    CHECK_EQ(out[0], 0x01); CHECK(n96_bitmask_get(&out[1], 0x04));

    n96_core_set_protocol(&c, 7);                  /* garbage value falls back to report protocol */
    CHECK_EQ(c.protocol, N96_PROTO_REPORT);
}

static void test_timeout_in_boot_protocol(void)
{
    n96_core_t c; n96_core_init(&c);
    uint8_t raw[16]; pkt(raw, 1, 0, 0x04);
    n96_core_on_payload(&c, raw, 16, 0);
    n96_core_set_protocol(&c, N96_PROTO_BOOT);
    CHECK(n96_core_tick(&c, N96_HOLD_TIMEOUT_MS));
    uint8_t out[N96_NKRO_REPORT_LEN];
    CHECK_EQ(n96_core_report(&c, out), 8);
    for (int i = 0; i < 8; i++) CHECK_EQ(out[i], 0);
}

int main(void)
{
    test_payload_updates_state(); test_seq_gap_counting(); test_timeout_releases_held_key();
    test_resends_keep_key_alive(); test_lost_release_recovered_two_ways();
    test_protocol_switch(); test_timeout_in_boot_protocol();
    TEST_MAIN_END("test_core");
}
