/*
 * Dongle main (STM32F405 + USB3300 ULPI PHY + nRF24L01+ module).
 * Stack: STM32Cube HAL (clocks/GPIO) + TinyUSB 0.19.0 (device, HID,
 * high-speed on the OTG_HS port = rhport 1). Pin plan: see board.c.
 *
 * WHAT THE "8 kHz" IS: the HID endpoint's bInterval=1 at High Speed = a
 * 125 us polling interval (see usb_descriptors.c). That is the USB leg
 * only. The RF link sends on key *changes*; the dongle forwards the newest
 * state at the next free endpoint slot. RF-leg latency is unmeasured, so
 * nothing here implies end-to-end 8 kHz.
 *
 * Build with -DN96_TEST_MODE=1 for the rate-test firmware: no radio, a
 * vendor-defined report that changes on every transfer, sent whenever the
 * endpoint is ready (see tools/rate_test.py).
 */
#include <string.h>
#include "tusb.h"
#include "board.h"
#include "nrf24l01p.h"
#include "report.h"
#include "core.h"

#ifndef N96_TEST_MODE
#define N96_TEST_MODE 0
#endif

/* ------------------------------------------------------------------ */
#if N96_TEST_MODE

static uint32_t test_seq;

uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t *b, uint16_t n)
{
    (void)i; (void)id; (void)t;
    if (n < N96_TEST_REPORT_LEN) return 0;
    n96_test_report(b, test_seq);
    return N96_TEST_REPORT_LEN;
}
void tud_hid_set_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t const *b, uint16_t n)
{ (void)i; (void)id; (void)t; (void)b; (void)n; }

int main(void)
{
    board_init();
    tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    tusb_init(N96_TUD_RHPORT, &dev_init);

    for (;;) {
        tud_task();
        if (tud_hid_ready()) {
            uint8_t rpt[N96_TEST_REPORT_LEN];
            n96_test_report(rpt, test_seq);
            if (tud_hid_report(0, rpt, sizeof rpt)) test_seq++;
        }
    }
}

/* ------------------------------------------------------------------ */
#else

#define N96_RF_CHANNEL 76            /* must match keyboard_esb_addon/esb_tx.c */
static const uint8_t rf_addr[5] = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 };

static const nrf24_hw_t nrf_hw = {
    .spi_xfer = board_spi3_xfer, .csn = board_nrf_csn, .ce = board_nrf_ce,
    .delay_ms = board_delay_ms,
};

_Static_assert(N96_PROTO_BOOT == HID_PROTOCOL_BOOT && N96_PROTO_REPORT == HID_PROTOCOL_REPORT,
               "core protocol values must match TinyUSB");

static n96_core_t core;
static bool radio_ok;

static void poll_radio(void)
{
    uint8_t buf[32];
    uint8_t len;
    while ((len = nrf24_read_payload(buf)) != 0) {
        n96_core_on_payload(&core, buf, len, board_millis());
        /* TODO: encoder / battery / heartbeat packets. */
    }
}

/* GET_REPORT (e.g. right after enumeration): answer with current state in
 * the active protocol's format. */
uint16_t tud_hid_get_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t *b, uint16_t n)
{
    (void)i; (void)id; (void)t;
    uint8_t rpt[N96_NKRO_REPORT_LEN];
    size_t len = n96_core_report(&core, rpt);
    if (n < len) return 0;
    memcpy(b, rpt, len);
    return (uint16_t)len;
}
void tud_hid_set_report_cb(uint8_t i, uint8_t id, hid_report_type_t t, uint8_t const *b, uint16_t n)
{ (void)i; (void)id; (void)t; (void)b; (void)n; /* no output report in the descriptor */ }

/* Host switched between boot (BIOS/UEFI) and report (OS driver) protocol. */
void tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol)
{
    (void)instance;
    n96_core_set_protocol(&core, protocol);
}

/* Re-send current state after (re-)enumeration so the host does not keep a stale one. */
void tud_mount_cb(void) { n96_core_mark_dirty(&core); }

int main(void)
{
    board_init();
    n96_core_init(&core);
    radio_ok = nrf24_init_prx(&nrf_hw, N96_RF_CHANNEL, rf_addr);   /* false: module not answering */
    tusb_rhport_init_t dev_init = { .role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO };
    tusb_init(N96_TUD_RHPORT, &dev_init);

    for (;;) {
        tud_task();
        if (radio_ok) poll_radio();
        n96_core_tick(&core, board_millis());   /* releases stuck keys if the link goes silent */
        if (core.dirty && tud_hid_ready()) {
            uint8_t rpt[N96_NKRO_REPORT_LEN];
            size_t len = n96_core_report(&core, rpt);
            if (tud_hid_report(0, rpt, (uint16_t)len)) core.dirty = false;
        }
    }
}

#endif
