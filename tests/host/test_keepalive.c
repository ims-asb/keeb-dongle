#include "check.h"
#include "keepalive.h"

static void bm_with(uint8_t bm[N96_KEY_BITMASK_BYTES], uint8_t usage)
{
    memset(bm, 0, N96_KEY_BITMASK_BYTES);
    if (usage) n96_bitmask_set(bm, usage, true);
}

static void test_held_detection(void)
{
    uint8_t bm[N96_KEY_BITMASK_BYTES];
    bm_with(bm, 0);
    CHECK(!n96_state_any_held(0, bm));
    CHECK(n96_state_any_held(0x02, bm));           /* modifier only */
    bm_with(bm, 103);
    CHECK(n96_state_any_held(0, bm));              /* last bit of last byte */
}

static void test_no_resend_when_idle(void)
{
    n96_resend_t r; n96_resend_init(&r);
    uint8_t bm[N96_KEY_BITMASK_BYTES]; bm_with(bm, 0);
    CHECK(!n96_resend_due(&r, 1000));              /* nothing sent yet */
    n96_resend_note_sent(&r, 0, bm, 1000);         /* all-keys-up state sent */
    CHECK(!n96_resend_due(&r, 1000 + 10 * N96_RESEND_INTERVAL_MS));   /* idle: never resend */
}

static void test_resend_while_held(void)
{
    n96_resend_t r; n96_resend_init(&r);
    uint8_t bm[N96_KEY_BITMASK_BYTES]; bm_with(bm, 0x04);
    n96_resend_note_sent(&r, 0, bm, 5000);
    CHECK(!n96_resend_due(&r, 5000));
    CHECK(!n96_resend_due(&r, 5000 + N96_RESEND_INTERVAL_MS - 1));
    CHECK(n96_resend_due(&r, 5000 + N96_RESEND_INTERVAL_MS));
    n96_resend_note_sent(&r, r.modifiers, r.bitmask, 5000 + N96_RESEND_INTERVAL_MS);   /* resent */
    CHECK(!n96_resend_due(&r, 5000 + N96_RESEND_INTERVAL_MS + 1));
    CHECK(n96_resend_due(&r, 5000 + 2 * N96_RESEND_INTERVAL_MS));
    /* modifier-only hold counts as held */
    n96_resend_t m; n96_resend_init(&m); bm_with(bm, 0);
    n96_resend_note_sent(&m, 0x01, bm, 0);
    CHECK(n96_resend_due(&m, N96_RESEND_INTERVAL_MS));
}

static void test_release_stops_resend(void)
{
    n96_resend_t r; n96_resend_init(&r);
    uint8_t bm[N96_KEY_BITMASK_BYTES]; bm_with(bm, 0x04);
    n96_resend_note_sent(&r, 0, bm, 0);
    CHECK(n96_resend_due(&r, 100));
    bm_with(bm, 0);
    n96_resend_note_sent(&r, 0, bm, 110);          /* release sent and accepted */
    CHECK(!n96_resend_due(&r, 110 + 10 * N96_RESEND_INTERVAL_MS));
}

static void test_failed_release_is_retried(void)
{
    n96_resend_t r; n96_resend_init(&r);
    uint8_t bm[N96_KEY_BITMASK_BYTES]; bm_with(bm, 0);
    n96_resend_note_sent(&r, 0, bm, 200);          /* release queued... */
    n96_resend_tx_failed(&r);                      /* ...and the radio gave up */
    CHECK(!n96_resend_due(&r, 200));               /* paced, not immediate */
    CHECK(n96_resend_due(&r, 200 + N96_RESEND_INTERVAL_MS));
    n96_resend_note_sent(&r, 0, bm, 220);          /* retry queued OK: flag cleared */
    CHECK(!n96_resend_due(&r, 220 + 5 * N96_RESEND_INTERVAL_MS));
    /* repeated failures keep retrying */
    n96_resend_tx_failed(&r);
    CHECK(n96_resend_due(&r, 220 + N96_RESEND_INTERVAL_MS));
}

static void test_wraparound(void)
{
    n96_resend_t r; n96_resend_init(&r);
    uint8_t bm[N96_KEY_BITMASK_BYTES]; bm_with(bm, 0x04);
    uint32_t t0 = 0xFFFFFFF0u;
    n96_resend_note_sent(&r, 0, bm, t0);
    CHECK(!n96_resend_due(&r, t0 + 5));            /* still before the interval (no wrap yet) */
    CHECK(n96_resend_due(&r, t0 + N96_RESEND_INTERVAL_MS));            /* wraps past 0 */
    CHECK(!n96_hold_timed_out(true, t0, t0 + N96_HOLD_TIMEOUT_MS - 1));
    CHECK(n96_hold_timed_out(true, t0, t0 + N96_HOLD_TIMEOUT_MS));
}

static void test_hold_timeout(void)
{
    CHECK(!n96_hold_timed_out(false, 0, 100000));  /* nothing held: never times out */
    CHECK(!n96_hold_timed_out(true, 1000, 1000));
    CHECK(!n96_hold_timed_out(true, 1000, 1000 + N96_HOLD_TIMEOUT_MS - 1));
    CHECK(n96_hold_timed_out(true, 1000, 1000 + N96_HOLD_TIMEOUT_MS));
}

/* Keyboard resend interval must leave room for several lost packets before the dongle times out. */
static void test_constants_relation(void)
{
    CHECK(N96_HOLD_TIMEOUT_MS / N96_RESEND_INTERVAL_MS >= 3);
}

int main(void)
{
    test_held_detection(); test_no_resend_when_idle(); test_resend_while_held();
    test_release_stops_resend(); test_failed_release_is_retried(); test_wraparound();
    test_hold_timeout(); test_constants_relation();
    TEST_MAIN_END("test_keepalive");
}
