#ifndef N96_ESB_TX_H
#define N96_ESB_TX_H
#include <stdint.h>
#include "protocol.h"

/* Initialise ESB as PTX (address/channel must match the dongle). 0 on success. */
int n96_esb_init(void);
/* Queue a key-state frame. Call on every key change. Returns esb_write_payload()'s result. */
int n96_esb_send_keys(uint8_t modifiers, const uint8_t key_bitmask[N96_KEY_BITMASK_BYTES]);
#endif
