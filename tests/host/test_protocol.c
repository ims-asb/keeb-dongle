#include "check.h"
#include "protocol.h"

static void test_layout(void)
{
    n96_key_state_packet_t p;
    CHECK_EQ(sizeof p, 16);
    CHECK_EQ(offsetof(n96_key_state_packet_t, type), 0);
    CHECK_EQ(offsetof(n96_key_state_packet_t, seq), 1);
    CHECK_EQ(offsetof(n96_key_state_packet_t, modifiers), 2);
    CHECK_EQ(offsetof(n96_key_state_packet_t, key_bitmask), 3);
    CHECK_EQ(sizeof(n96_encoder_packet_t), 4);
    CHECK_EQ(N96_KEY_BITMASK_BYTES * 8, N96_KEY_USAGE_MAX + 1);
}

static void test_bitmask(void)
{
    uint8_t bm[N96_KEY_BITMASK_BYTES] = {0};
    CHECK(n96_bitmask_set(bm, 0x04, true));          /* 'a' */
    CHECK_EQ(bm[0], 0x10);
    CHECK(n96_bitmask_set(bm, 0, true));             /* usage 0 is representable */
    CHECK_EQ(bm[0], 0x11);
    CHECK(n96_bitmask_set(bm, 103, true));           /* highest */
    CHECK_EQ(bm[12], 0x80);
    CHECK(n96_bitmask_get(bm, 103));
    CHECK(!n96_bitmask_get(bm, 102));
    CHECK(n96_bitmask_set(bm, 0x04, false));
    CHECK_EQ(bm[0], 0x01);
    /* out of range: refused, nothing changes */
    uint8_t before[N96_KEY_BITMASK_BYTES]; memcpy(before, bm, sizeof bm);
    CHECK(!n96_bitmask_set(bm, 104, true));
    CHECK(!n96_bitmask_set(bm, 224, true));          /* modifier usage belongs in the modifiers byte */
    CHECK(!n96_bitmask_set(bm, 255, true));
    CHECK(memcmp(bm, before, sizeof bm) == 0);
    CHECK(!n96_bitmask_get(bm, 200));
}

static void test_pack_unpack_roundtrip(void)
{
    uint8_t bm[N96_KEY_BITMASK_BYTES];
    for (int i = 0; i < N96_KEY_BITMASK_BYTES; i++) bm[i] = (uint8_t)(0xA0 + i);
    n96_key_state_packet_t p, q;
    CHECK_EQ(n96_pack_key_state(&p, 42, 0x81, bm), 16);
    const uint8_t *raw = (const uint8_t *)&p;
    CHECK_EQ(raw[0], N96_PKT_KEY_STATE);
    CHECK_EQ(raw[1], 42);
    CHECK_EQ(raw[2], 0x81);
    CHECK(memcmp(&raw[3], bm, sizeof bm) == 0);
    CHECK(n96_unpack_key_state(&q, raw, 16));
    CHECK(memcmp(&p, &q, sizeof p) == 0);
}

static void test_unpack_rejects(void)
{
    uint8_t raw[32] = { N96_PKT_KEY_STATE };
    n96_key_state_packet_t q;
    CHECK(n96_unpack_key_state(&q, raw, 16));
    CHECK(!n96_unpack_key_state(&q, raw, 15));       /* short */
    CHECK(!n96_unpack_key_state(&q, raw, 17));       /* long */
    CHECK(!n96_unpack_key_state(&q, raw, 0));
    raw[0] = N96_PKT_HEARTBEAT;
    CHECK(!n96_unpack_key_state(&q, raw, 16));       /* wrong type */
    raw[0] = 0x00;
    CHECK(!n96_unpack_key_state(&q, raw, 16));
}

static void test_seq_missed(void)
{
    CHECK_EQ(n96_seq_missed(10, 11), 0);
    CHECK_EQ(n96_seq_missed(10, 12), 1);
    CHECK_EQ(n96_seq_missed(10, 10), 255);           /* duplicate looks like 255 missed; documented limitation */
    CHECK_EQ(n96_seq_missed(255, 0), 0);             /* wrap */
    CHECK_EQ(n96_seq_missed(254, 1), 2);
}

int main(void)
{
    test_layout(); test_bitmask(); test_pack_unpack_roundtrip();
    test_unpack_rejects(); test_seq_missed();
    TEST_MAIN_END("test_protocol");
}
