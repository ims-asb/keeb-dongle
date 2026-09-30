/*
 * keepalive.h - logic that closes the "stuck key" gap, shared by both sides
 * and free of hardware dependencies so it is unit-tested on the host.
 *
 * Problem: the radio link sends on key changes only. If the packet carrying
 * the last key-release is lost (esb_tx.c flushes the TX FIFO on TX_FAILED),
 * the dongle would hold that key forever.
 *
 * Two mechanisms, both needed:
 *  - Keyboard: while any key is held (or after a failed transmission),
 *    re-send the current full state every N96_RESEND_INTERVAL_MS.
 *  - Dongle: if any key is held and no key-state packet has arrived for
 *    N96_HOLD_TIMEOUT_MS, release everything (covers a lost release, a
 *    keyboard that powered off, or a dead link).
 *
 * The two constants are engineering guesses, NOT measured on any link: the
 * timeout allows about five consecutive lost resends before a false
 * release. Tune them once RF loss and latency have been measured
 * (DECISIONS.md, problem 2). Time arguments are free-running millisecond
 * counters; all arithmetic is wrap-safe (uint32).
 */
#ifndef N96_KEEPALIVE_H
#define N96_KEEPALIVE_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "protocol.h"

#define N96_RESEND_INTERVAL_MS  20u
#define N96_HOLD_TIMEOUT_MS    100u

_Static_assert(N96_HOLD_TIMEOUT_MS >= 3 * N96_RESEND_INTERVAL_MS,
               "timeout must span several resend intervals");

/* True if any modifier bit or any key bit is set. */
static inline bool n96_state_any_held(uint8_t modifiers,
                                      const uint8_t bitmask[N96_KEY_BITMASK_BYTES])
{
    if (modifiers) return true;
    for (unsigned i = 0; i < N96_KEY_BITMASK_BYTES; i++)
        if (bitmask[i]) return true;
    return false;
}

/* ---- keyboard side ---------------------------------------------------- */

typedef struct {
    uint8_t  modifiers;
    uint8_t  bitmask[N96_KEY_BITMASK_BYTES];
    uint32_t last_tx_ms;
    bool     have_state;
    volatile bool retry;     /* set from the ESB event handler on TX_FAILED */
} n96_resend_t;

static inline void n96_resend_init(n96_resend_t *r) { memset((void *)r, 0, sizeof *r); }

/* Record the state that was just handed to the radio (call on every send). */
static inline void n96_resend_note_sent(n96_resend_t *r, uint8_t modifiers,
                                        const uint8_t bitmask[N96_KEY_BITMASK_BYTES],
                                        uint32_t now_ms)
{
    r->modifiers = modifiers;
    memcpy(r->bitmask, bitmask, N96_KEY_BITMASK_BYTES);
    r->last_tx_ms = now_ms;
    r->have_state = true;
    r->retry = false;
}

/* The radio gave up on the last packet (state update lost). */
static inline void n96_resend_tx_failed(n96_resend_t *r) { r->retry = true; }

/* True when the current state should be re-sent now: keys held or a failed
 * transmission pending, and at least one interval since the last send. */
static inline bool n96_resend_due(const n96_resend_t *r, uint32_t now_ms)
{
    if (!r->have_state) return false;
    if (!(r->retry || n96_state_any_held(r->modifiers, r->bitmask))) return false;
    return (uint32_t)(now_ms - r->last_tx_ms) >= N96_RESEND_INTERVAL_MS;
}

/* ---- dongle side ------------------------------------------------------ */

/* True if keys are held and the link has been silent for the timeout. */
static inline bool n96_hold_timed_out(bool keys_held, uint32_t last_rx_ms, uint32_t now_ms)
{
    return keys_held && (uint32_t)(now_ms - last_rx_ms) >= N96_HOLD_TIMEOUT_MS;
}

#endif /* N96_KEEPALIVE_H */
