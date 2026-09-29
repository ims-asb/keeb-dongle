/* STM32F405 dongle board support. See board.c for the pin plan. */
#ifndef N96_BOARD_H
#define N96_BOARD_H
#include <stdint.h>
#include <stdbool.h>

void     board_init(void);           /* clocks, GPIO, ULPI pins (AF10), SPI3, SysTick */
uint8_t  board_spi3_xfer(uint8_t b); /* nRF24 SPI, mode 0, blocking */
void     board_nrf_csn(bool level);
void     board_nrf_ce(bool level);
void     board_delay_ms(uint32_t ms);
uint32_t board_millis(void);
/* 96-bit unique ID as 24 hex chars + NUL, for the USB serial string. */
void     board_uid_hex(char out[25]);

#endif
