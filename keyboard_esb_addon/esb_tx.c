/*
 * esb_tx.c - keyboard-side transmitter (nRF52840) using Nordic's
 * Enhanced ShockBurst (ESB), NOT Gazell.
 *
 * WHY ESB: the dongle uses an nRF24L01+ module, which speaks ESB in
 * hardware. Nordic's docs state the nRF5 ESB library is on-air
 * compatible with nRF24L devices (use the ESB legacy config for
 * compatibility). Gazell is a different protocol - an earlier draft of
 * this project used it by mistake.
 *
 * API names (esb_init, esb_write_payload, esb_set_base_address_0,
 * esb_set_prefixes, esb_set_rf_channel, struct esb_payload/esb_config)
 * are from the nRF Connect SDK ESB docs. Field names inside the structs
 * and the exact Kconfig (CONFIG_ESB=y, legacy-compat option) should be
 * checked against the SDK version you actually build with.
 *
 * STATUS: untested skeleton. Whether this can share the nRF52840 with
 * ZMK's Bluetooth stack is still an open question (radio time-slicing).
 */
#include <zephyr/kernel.h>
#include <esb.h>
#include <string.h>
#include "protocol.h"

// Must match the dongle's nRF24L01+ RX_ADDR_P0 (5 bytes) and channel.
// All-equal bytes on purpose: avoids byte-order mismatches between the
// nRF5 ESB address registers and the nRF24 address registers.
#define N96_ADDR_BYTE   0xE7
#define N96_RF_CHANNEL  76   /* TODO: pick a quiet channel; must match dongle */

static struct esb_payload tx_payload;
static uint8_t seq_counter;

static void esb_event_handler(struct esb_evt const *event)
{
    switch (event->evt_id) {
    case ESB_EVENT_TX_SUCCESS:
        break;                    /* dongle ACKed */
    case ESB_EVENT_TX_FAILED:
        esb_flush_tx();           /* drop stale state; a fresh one follows */
        break;
    default:
        break;
    }
}

int n96_esb_init(void)
{
    struct esb_config config = ESB_DEFAULT_CONFIG;

    config.protocol      = ESB_PROTOCOL_ESB_DPL;   /* dynamic payload = nRF24 DPL */
    config.mode          = ESB_MODE_PTX;
    config.bitrate       = ESB_BITRATE_2MBPS;
    config.crc           = ESB_CRC_16BIT;          /* nRF24 CRCO=1 */
    config.event_handler = esb_event_handler;
    config.retransmit_count = 3;                   /* TODO: tune */
    config.selective_auto_ack = false;

    int err = esb_init(&config);
    if (err) { return err; }

    uint8_t base[4]   = { N96_ADDR_BYTE, N96_ADDR_BYTE, N96_ADDR_BYTE, N96_ADDR_BYTE };
    uint8_t prefix[1] = { N96_ADDR_BYTE };
    esb_set_base_address_0(base);
    esb_set_prefixes(prefix, 1);
    esb_set_rf_channel(N96_RF_CHANNEL);
    return 0;
}

/* Call whenever the key state changes (event-driven, not on a timer). */
int n96_esb_send_keys(uint8_t modifiers, const uint8_t key_bitmask[13])
{
    n96_key_state_packet_t pkt = {
        .type = N96_PKT_KEY_STATE,
        .seq = seq_counter++,
        .modifiers = modifiers,
    };
    memcpy(pkt.key_bitmask, key_bitmask, sizeof(pkt.key_bitmask));

    tx_payload.length = sizeof(pkt);
    tx_payload.pipe   = 0;
    tx_payload.noack  = false;
    memcpy(tx_payload.data, &pkt, sizeof(pkt));
    return esb_write_payload(&tx_payload);
}
