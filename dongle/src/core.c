#include "core.h"
#include <string.h>
#include "keepalive.h"

void n96_core_init(n96_core_t *c)
{
    memset(c, 0, sizeof *c);
    c->protocol = N96_PROTO_REPORT;    /* USB HID default until the host says otherwise */
}

bool n96_core_on_payload(n96_core_t *c, const uint8_t *payload, size_t len, uint32_t now_ms)
{
    if (!n96_report_from_payload(c->nkro, payload, len)) return false;
    if (c->have_seq) c->rf_missed += n96_seq_missed(c->last_seq, payload[1]);
    c->last_seq = payload[1];
    c->have_seq = true;
    c->last_rx_ms = now_ms;
    c->dirty = true;
    return true;
}

bool n96_core_tick(n96_core_t *c, uint32_t now_ms)
{
    if (!n96_hold_timed_out(n96_report_any_held(c->nkro), c->last_rx_ms, now_ms)) return false;
    memset(c->nkro, 0, sizeof c->nkro);
    c->timeouts++;
    c->dirty = true;
    return true;
}

void n96_core_set_protocol(n96_core_t *c, uint8_t protocol)
{
    c->protocol = (protocol == N96_PROTO_BOOT) ? N96_PROTO_BOOT : N96_PROTO_REPORT;
    c->dirty = true;
}

void n96_core_mark_dirty(n96_core_t *c) { c->dirty = true; }

size_t n96_core_report(const n96_core_t *c, uint8_t *out)
{
    if (c->protocol == N96_PROTO_BOOT) {
        n96_boot_report_from_nkro(out, c->nkro);
        return N96_BOOT_REPORT_LEN;
    }
    memcpy(out, c->nkro, N96_NKRO_REPORT_LEN);
    return N96_NKRO_REPORT_LEN;
}
