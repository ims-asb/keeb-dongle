/*
 * protocol.h — shared packet format between the Nano96 (Gazell TX) and
 * the receiver dongle (Gazell RX).
 *
 * STATUS: skeleton / not yet validated on real hardware. This defines
 * the wire format both sides need to agree on; nothing here has been
 * tested for actual RF timing or throughput yet.
 */

#ifndef NANO96_GZLL_PROTOCOL_H
#define NANO96_GZLL_PROTOCOL_H

#include <stdint.h>

// ESB (legacy nRF24 mode) payload max is 32 bytes. Using 20 here
// gives headroom without wasting airtime on unused bytes.
#define N96_PACKET_MAX_LEN   20

// Packet types — lets the dongle tell a full key-state frame apart
// from things like battery level or a heartbeat/keepalive.
typedef enum {
    N96_PKT_KEY_STATE   = 0x01,
    N96_PKT_ENCODER      = 0x02,
    N96_PKT_BATTERY      = 0x03,
    N96_PKT_HEARTBEAT    = 0x04,
} n96_packet_type_t;

// Key state frame: bitmask of currently-pressed keys, one bit per key.
// 97 keys needs ceil(97/8) = 13 bytes. Correction from an earlier
// draft of this file: 1 (type) + 1 (seq) + 1 (modifiers) + 13 (bitmask)
// = 16 bytes total, still fits inside a 32-byte Gazell payload with
// room to spare — no delta-encoding needed.
//
// modifiers and key_bitmask are split because standard USB HID NKRO
// descriptors treat them separately: modifiers (Ctrl/Shift/Alt/GUI)
// are HID usage IDs 224-231, reported as their own byte, while the
// key bitmap covers usage IDs 0-103 in a separate block. See
// hid_nkro_descriptor.h on the dongle side for the matching layout.
//
// key_bitmask bit N is assumed to directly equal HID keyboard usage
// ID N (0-103) — i.e. the keyboard side is responsible for mapping
// its own physical key positions to standard HID usage IDs before
// setting bits here. That mapping table itself is NOT built yet.
typedef struct __attribute__((packed)) {
    uint8_t  type;             // n96_packet_type_t
    uint8_t  seq;               // rolling sequence number, detects drops
    uint8_t  modifiers;         // bit0=LCtrl,1=LShift,2=LAlt,3=LGUI,4=RCtrl,5=RShift,6=RAlt,7=RGUI
    uint8_t  key_bitmask[13];   // bit N = HID usage ID N (0-103), non-modifier keys
} n96_key_state_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  seq;
    int8_t   delta;         // encoder tick delta since last packet
    uint8_t  pressed;       // encoder pushbutton state
} n96_encoder_packet_t;

#endif // NANO96_GZLL_PROTOCOL_H
