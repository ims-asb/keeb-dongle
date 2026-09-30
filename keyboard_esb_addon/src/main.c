/*
 * Bring-up app for the keyboard-side ESB transmitter. Sends a walking
 * single-key report every 100 ms so the dongle side has something to
 * receive. This is NOT the ZMK integration; it exists so esb_tx.c can be
 * built and, later, exercised on a bare nRF52840 board.
 */
#include <zephyr/kernel.h>
#include "esb_tx.h"

int main(void)
{
    if (n96_esb_init() != 0) {
        return 0;
    }

    uint8_t usage = 0x04;                 /* HID 'a' */
    for (;;) {
        uint8_t bitmask[N96_KEY_BITMASK_BYTES] = { 0 };
        n96_bitmask_set(bitmask, usage, true);
        n96_esb_send_keys(0, bitmask);
        usage = (usage >= 0x1D) ? 0x04 : usage + 1;   /* a..z */
        for (int i = 0; i < 20; i++) {       /* 100 ms between key changes, ticking every 5 ms */
            n96_esb_tick();
            k_sleep(K_MSEC(5));
        }
    }
    return 0;
}
