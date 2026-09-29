/*
 * protocol.h - shared packet format between the keyboard (ESB PTX,
 * nRF52840) and the dongle (nRF24L01+ PRX).
 *
 * STATUS: wire format only. It has host-side unit tests (tests/host) but
 * has never crossed a real RF link; nothing here says anything about RF
 * timing or throughput.
 */

#ifndef N96_PROTOCOL_H
#define N96_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

/* nRF24 / ESB payload max is 32 bytes. */
#define N96_PACKET_MAX_LEN   20

/* Packet types - lets the dongle tell a full key-state frame apart
 * from things like battery level or a heartbeat/keepalive. */
typedef enum {
    N96_PKT_KEY_STATE   = 0x01,
    N96_PKT_ENCODER     = 0x02,
    N96_PKT_BATTERY     = 0x03,
    N96_PKT_HEARTBEAT   = 0x04,
} n96_packet_type_t;

/* Key-state frame: bitmask of currently-pressed keys, one bit per key.
 * 1 (type) + 1 (seq) + 1 (modifiers) + 13 (bitmask) = 16 bytes, well inside
 * the 32-byte ESB payload, so no delta encoding.
 *
 * modifiers and key_bitmask are split because the NKRO HID descriptor
 * (dongle/src/hid_nkro_descriptor.h) treats them separately: modifiers are
 * usage IDs 224-231 sent as their own byte, the bitmap covers usage IDs
 * 0-103.
 *
 * key_bitmask bit N == HID keyboard usage ID N (0-103). The keyboard side
 * is responsible for mapping physical key positions to usage IDs before
 * setting bits. That mapping table is NOT built (no KLE/KiCad source in
 * the repo). */
#define N96_KEY_BITMASK_BYTES 13
#define N96_KEY_USAGE_MAX     103   /* highest usage ID representable */

typedef struct __attribute__((packed)) {
    uint8_t  type;             /* n96_packet_type_t */
    uint8_t  seq;              /* rolling sequence number, detects drops */
    uint8_t  modifiers;        /* bit0=LCtrl,1=LShift,2=LAlt,3=LGUI,4=RCtrl,5=RShift,6=RAlt,7=RGUI */
    uint8_t  key_bitmask[N96_KEY_BITMASK_BYTES]; /* bit N = usage ID N (0-103) */
} n96_key_state_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  seq;
    int8_t   delta;         /* encoder tick delta since last packet */
    uint8_t  pressed;       /* encoder pushbutton state */
} n96_encoder_packet_t;

_Static_assert(sizeof(n96_key_state_packet_t) == 16, "key state packet must be 16 bytes");
_Static_assert(sizeof(n96_key_state_packet_t) <= N96_PACKET_MAX_LEN, "exceeds N96_PACKET_MAX_LEN");
_Static_assert(sizeof(n96_encoder_packet_t) == 4, "encoder packet must be 4 bytes");

/* Set usage ID `usage` (0..N96_KEY_USAGE_MAX) in a key bitmask. Returns
 * false (and changes nothing) for an out-of-range usage. Modifier usage IDs
 * (224-231) do not belong in the bitmask; use the modifiers byte. */
static inline bool n96_bitmask_set(uint8_t bitmask[N96_KEY_BITMASK_BYTES], uint8_t usage, bool pressed)
{
    if (usage > N96_KEY_USAGE_MAX) return false;
    if (pressed) bitmask[usage >> 3] |= (uint8_t)(1u << (usage & 7));
    else         bitmask[usage >> 3] &= (uint8_t)~(1u << (usage & 7));
    return true;
}

static inline bool n96_bitmask_get(const uint8_t bitmask[N96_KEY_BITMASK_BYTES], uint8_t usage)
{
    if (usage > N96_KEY_USAGE_MAX) return false;
    return (bitmask[usage >> 3] >> (usage & 7)) & 1u;
}

/* Fill a key-state packet. 13 bytes = 104 bits = usages 0..103 exactly, so
 * there are no spare bits to mask. Returns the number of bytes to transmit. */
static inline size_t n96_pack_key_state(n96_key_state_packet_t *out, uint8_t seq,
                                        uint8_t modifiers,
                                        const uint8_t bitmask[N96_KEY_BITMASK_BYTES])
{
    out->type = N96_PKT_KEY_STATE;
    out->seq = seq;
    out->modifiers = modifiers;
    memcpy(out->key_bitmask, bitmask, N96_KEY_BITMASK_BYTES);
    return sizeof(*out);
}

/* Parse a received payload. Returns false unless it is exactly one
 * well-formed key-state packet. */
static inline bool n96_unpack_key_state(n96_key_state_packet_t *out, const uint8_t *buf, size_t len)
{
    if (len != sizeof(*out) || buf[0] != N96_PKT_KEY_STATE) return false;
    memcpy(out, buf, sizeof(*out));
    return true;
}

/* Number of packets missed between two consecutively received sequence
 * numbers (0 if `cur` directly follows `last`). Wraps at 256. Note: a gap of
 * a multiple of 256 is indistinguishable from no loss. */
static inline uint8_t n96_seq_missed(uint8_t last, uint8_t cur)
{
    return (uint8_t)(cur - last - 1u);
}

#endif /* N96_PROTOCOL_H */
