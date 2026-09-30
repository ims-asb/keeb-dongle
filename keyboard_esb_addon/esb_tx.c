/*
 * esb_tx.c - keyboard-side transmitter (nRF52840) using Nordic's
 * Enhanced ShockBurst (ESB), NOT Gazell.
 *
 * WHY ESB: the dongle uses an nRF24L01+ module, which speaks ESB in
 * hardware. Nordic's NCS docs say the ESB library can talk to nRF24L
 * devices; the documented compatibility recipe is ESB_LEGACY_CONFIG
 * (fixed payload, 8-bit CRC, nRFgo-SDK style). This file instead uses
 * dynamic payload + 16-bit CRC, which the nRF24L01+ also implements, but
 * that exact combination is NOT documented as tested against nRF24
 * hardware - see VERIFY.md.
 *
 * API and struct field names were checked against nrfconnect/sdk-nrf
 * include/esb.h (v3.4.1 == main at the time of checking). Kconfig needed:
 * CONFIG_ESB=y, CONFIG_ESB_CLOCK_INIT=y (starts HFCLK inside esb_init).
 *
 * STATUS: compiles only (see SUMMARY.md); never run. Whether this can
 * share the nRF52840 with ZMK's Bluetooth stack is an open problem -
 * see DECISIONS.md.
 */
#include <zephyr/kernel.h>
#include <esb.h>
#include <string.h>
#include "esb_tx.h"
#include "keepalive.h"

/* Must match the dongle's nRF24L01+ RX_ADDR_P0 (5 bytes) and channel.
 * All-equal bytes on purpose: avoids byte-order mismatches between the
 * nRF5 ESB address registers and the nRF24 address registers. */
#define N96_ADDR_BYTE   0xE7
#define N96_RF_CHANNEL  76   /* TODO: pick a quiet channel; must match dongle */

static struct esb_payload tx_payload;
static uint8_t seq_counter;
static n96_resend_t resend;   /* stuck-key protection: see shared/keepalive.h */

static void n96_esb_evt_cb(struct esb_evt const *event)
{
    switch (event->evt_id) {
    case ESB_EVENT_TX_SUCCESS:
        break;                    /* dongle ACKed */
    case ESB_EVENT_TX_FAILED:
        /* The failed payload stays at the head of the TX FIFO; flush it.
         * That drops the state update, so ask n96_esb_tick() to re-send the
         * current state (paced by N96_RESEND_INTERVAL_MS). The dongle's hold
         * timeout is the backstop if the link stays down. */
        esb_flush_tx();
        n96_resend_tx_failed(&resend);
        break;
    default:
        break;
    }
}

int n96_esb_init(void)
{
    n96_resend_init(&resend);
    struct esb_config config = ESB_DEFAULT_CONFIG;

    config.protocol      = ESB_PROTOCOL_ESB_DPL;   /* dynamic payload = nRF24 DPL */
    config.mode          = ESB_MODE_PTX;
    config.bitrate       = ESB_BITRATE_2MBPS;
    config.crc           = ESB_CRC_16BIT;          /* nRF24 CRCO=1 */
    config.event_handler = n96_esb_evt_cb;
    config.retransmit_count = 3;                   /* TODO: tune */
    config.selective_auto_ack = false;

    int err = esb_init(&config);
    if (err) { return err; }

    uint8_t base[4]   = { N96_ADDR_BYTE, N96_ADDR_BYTE, N96_ADDR_BYTE, N96_ADDR_BYTE };
    uint8_t prefix[1] = { N96_ADDR_BYTE };
    err = esb_set_base_address_0(base);
    if (err) { return err; }
    err = esb_set_prefixes(prefix, 1);
    if (err) { return err; }
    return esb_set_rf_channel(N96_RF_CHANNEL);
}

static int send_state(uint8_t modifiers, const uint8_t key_bitmask[N96_KEY_BITMASK_BYTES])
{
    n96_key_state_packet_t pkt;
    size_t len = n96_pack_key_state(&pkt, seq_counter++, modifiers, key_bitmask);

    tx_payload.length = len;
    tx_payload.pipe   = 0;
    tx_payload.noack  = false;
    memcpy(tx_payload.data, &pkt, len);
    int err = esb_write_payload(&tx_payload);
    /* Remember the state either way so the tick can re-send it. */
    n96_resend_note_sent(&resend, modifiers, key_bitmask, k_uptime_get_32());
    if (err) {
        n96_resend_tx_failed(&resend);      /* e.g. TX FIFO full: retry on a later tick */
    }
    return err;
}

/* Call whenever the key state changes (event-driven). */
int n96_esb_send_keys(uint8_t modifiers, const uint8_t key_bitmask[N96_KEY_BITMASK_BYTES])
{
    return send_state(modifiers, key_bitmask);
}

/* Call regularly (a few ms is plenty): re-sends the current state every
 * N96_RESEND_INTERVAL_MS while any key is held or after a failed transmission. */
void n96_esb_tick(void)
{
    if (n96_resend_due(&resend, k_uptime_get_32())) {
        uint8_t mods = resend.modifiers;
        uint8_t bm[N96_KEY_BITMASK_BYTES];
        memcpy(bm, resend.bitmask, sizeof bm);
        (void)send_state(mods, bm);
    }
}
