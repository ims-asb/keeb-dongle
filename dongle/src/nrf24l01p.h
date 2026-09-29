#ifndef NRF24L01P_H
#define NRF24L01P_H
#include <stdint.h>
#include <stdbool.h>

/* Hardware hooks - implemented in the STM32 board file (or a test mock). */
typedef struct {
    uint8_t (*spi_xfer)(uint8_t tx);   /* full-duplex 1 byte */
    void    (*csn)(bool level);
    void    (*ce)(bool level);
    void    (*delay_ms)(uint32_t ms);
} nrf24_hw_t;

/* Configure the module as a primary receiver and start listening.
 * Returns false if the CONFIG/RF_CH read-back does not match what was
 * written (module absent, wiring fault, SPI mode wrong). */
bool nrf24_init_prx(const nrf24_hw_t *hw, uint8_t channel, const uint8_t addr[5]);
/* Returns payload length (1..32) and fills buf, or 0 if nothing waiting. */
uint8_t nrf24_read_payload(uint8_t *buf);
#endif
