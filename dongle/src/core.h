/*
 * Dongle core logic: radio payloads -> current key state -> HID report,
 * with the hold-timeout watchdog and HID boot/report protocol selection.
 * Hardware-free (time is passed in), so it is unit-tested on the host;
 * main.c only wires it to the nRF24, TinyUSB and the millisecond clock.
 */
#ifndef N96_CORE_H
#define N96_CORE_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "report.h"

/* Values match HID_PROTOCOL_BOOT / HID_PROTOCOL_REPORT (checked in main.c). */
#define N96_PROTO_BOOT   0
#define N96_PROTO_REPORT 1

typedef struct {
    uint8_t  nkro[N96_NKRO_REPORT_LEN];   /* current key state, NKRO layout */
    bool     dirty;                       /* a report should be sent */
    uint8_t  protocol;                    /* N96_PROTO_* */
    uint8_t  last_seq;
    bool     have_seq;
    uint32_t last_rx_ms;                  /* last valid key-state packet */
    uint32_t rf_missed;                   /* diagnostics: sequence gaps */
    uint32_t timeouts;                    /* diagnostics: hold-timeout releases */
} n96_core_t;

void n96_core_init(n96_core_t *c);

/* Feed one raw radio payload. Returns true if it was a valid key-state packet. */
bool n96_core_on_payload(n96_core_t *c, const uint8_t *payload, size_t len, uint32_t now_ms);

/* Call regularly. Releases all keys if any are held and the link has been
 * silent for N96_HOLD_TIMEOUT_MS. Returns true if it did. */
bool n96_core_tick(n96_core_t *c, uint32_t now_ms);

/* Host changed protocol (SET_PROTOCOL) or (re)enumerated: resend current state. */
void n96_core_set_protocol(n96_core_t *c, uint8_t protocol);
void n96_core_mark_dirty(n96_core_t *c);

/* Build the report for the active protocol into `out` (>= N96_NKRO_REPORT_LEN
 * bytes); returns its length (14 in report protocol, 8 in boot protocol). */
size_t n96_core_report(const n96_core_t *c, uint8_t *out);

#endif
