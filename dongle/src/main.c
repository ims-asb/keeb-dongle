/*
 * Dongle main (STM32F405 + USB3300 ULPI PHY + nRF24L01+ module).
 * Stack assumption: STM32Cube HAL + TinyUSB (device, HID, high-speed on
 * the OTG_HS port). Written against the TinyUSB API as I know it -
 * verify names against the TinyUSB version you vendor.
 *
 * PIN PLAN (chosen to avoid ULPI conflicts):
 *   ULPI (AF10): CK=PA5 D0=PA3 D1=PB0 D2=PB1 D3=PB10 D4=PB11 D5=PB12
 *                D6=PB13 D7=PB5 STP=PC0 DIR=PC2 NXT=PC3
 *   nRF24 on SPI3 (AF6): SCK=PC10 MISO=PC11 MOSI=PC12
 *                CSN=PA4 CE=PA6 IRQ=PA7 (IRQ optional; we poll)
 *   NOTE: SPI1's default SCK is PA5 = ULPI_CK, and SPI3's PB3-5 option
 *   collides with ULPI_D7 (PB5) - that's why SPI3 uses PC10-12.
 *
 * WHAT ACTUALLY GIVES YOU 8 kHz: the HID endpoint's bInterval=1 at High
 * Speed = 125 us microframe (see usb_descriptors.c). The RF link only
 * sends on key *changes*; the dongle then sends a USB report at the next
 * microframe. Latency gain = up to 1 ms -> 0.125 ms on the USB leg.
 * (RF leg latency is separate and unmeasured.)
 */
#include <string.h>
#include "tusb.h"
#include "nrf24l01p.h"
#include "hid_nkro_descriptor.h"
#include "protocol.h"

#define N96_RF_CHANNEL 76            /* must match esb_tx.c */
static const uint8_t rf_addr[5] = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 };

/* TODO: implement in board file using HAL: SPI3 xfer, GPIO for CSN/CE. */
extern uint8_t board_spi3_xfer(uint8_t b);
extern void    board_nrf_csn(bool level);
extern void    board_nrf_ce(bool level);
extern void    board_init(void);      /* clocks, ULPI pins AF10, SPI3, GPIO */

static const nrf24_hw_t nrf_hw = {
    .spi_xfer = board_spi3_xfer, .csn = board_nrf_csn, .ce = board_nrf_ce,
};

static uint8_t report[N96_NKRO_REPORT_LEN];
static volatile bool report_dirty;
static uint8_t last_seq; static bool have_seq;

static void poll_radio(void)
{
    uint8_t buf[32];
    uint8_t len;
    while ((len = nrf24_read_payload(buf)) != 0) {
        if (buf[0] == N96_PKT_KEY_STATE && len == sizeof(n96_key_state_packet_t)) {
            const n96_key_state_packet_t *p = (const void *)buf;
            /* TODO: use p->seq to count dropped packets (diagnostics). */
            (void)last_seq; (void)have_seq;
            report[0] = p->modifiers;
            memcpy(&report[1], p->key_bitmask, N96_NKRO_KEY_BYTES);
            report_dirty = true;
        }
        /* TODO: encoder / battery / heartbeat packets. */
    }
}

int main(void)
{
    board_init();
    nrf24_init_prx(&nrf_hw, N96_RF_CHANNEL, rf_addr);
    tusb_init();   /* TinyUSB config: device mode, HS on OTG_HS port - see tusb_config note */

    while (1) {
        tud_task();
        poll_radio();
        if (report_dirty && tud_hid_ready()) {
            if (tud_hid_report(0, report, sizeof(report))) report_dirty = false;
        }
        /* TODO: send a fresh report if the host asked (GET_REPORT) and on
         * wake-up / re-enumeration so stuck keys are cleared. */
    }
}
