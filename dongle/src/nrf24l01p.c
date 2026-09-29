/*
 * Minimal nRF24L01+ driver (primary receiver, dynamic payloads, 2 Mbps,
 * 16-bit CRC, auto-ack on pipe 0) to match the keyboard's ESB_PROTOCOL_ESB_DPL.
 *
 * Register/command values are from the nRF24L01+ datasheet as I know it -
 * NOT re-verified against the PDF in this session. Cross-check the
 * register map before trusting bring-up results.
 */
#include "nrf24l01p.h"

#define CMD_R_REGISTER    0x00
#define CMD_W_REGISTER    0x20
#define CMD_R_RX_PL_WID   0x60
#define CMD_R_RX_PAYLOAD  0x61
#define CMD_FLUSH_TX      0xE1
#define CMD_FLUSH_RX      0xE2

#define REG_CONFIG        0x00
#define REG_EN_AA         0x01
#define REG_EN_RXADDR     0x02
#define REG_SETUP_AW      0x03
#define REG_RF_CH         0x05
#define REG_RF_SETUP      0x06
#define REG_STATUS        0x07
#define REG_RX_ADDR_P0    0x0A
#define REG_FIFO_STATUS   0x17
#define REG_DYNPD         0x1C
#define REG_FEATURE       0x1D

#define STATUS_RX_DR      0x40
#define FIFO_RX_EMPTY     0x01

static const nrf24_hw_t *hw;

static void wr(uint8_t reg, const uint8_t *v, uint8_t n)
{
    hw->csn(false);
    hw->spi_xfer(CMD_W_REGISTER | reg);
    for (uint8_t i = 0; i < n; i++) hw->spi_xfer(v[i]);
    hw->csn(true);
}
static void wr1(uint8_t reg, uint8_t v) { wr(reg, &v, 1); }
static uint8_t rd1(uint8_t reg)
{
    hw->csn(false);
    hw->spi_xfer(CMD_R_REGISTER | reg);
    uint8_t v = hw->spi_xfer(0xFF);
    hw->csn(true);
    return v;
}
static void cmd(uint8_t c) { hw->csn(false); hw->spi_xfer(c); hw->csn(true); }

void nrf24_init_prx(const nrf24_hw_t *h, uint8_t channel, const uint8_t addr[5])
{
    hw = h;
    hw->ce(false);
    /* TODO: wait ~100 ms after power-on before first SPI access (datasheet). */

    wr1(REG_CONFIG,    0x0F);  /* PWR_UP | PRIM_RX | EN_CRC | CRCO(16-bit) */
    wr1(REG_EN_AA,     0x01);  /* auto-ack pipe 0 */
    wr1(REG_EN_RXADDR, 0x01);  /* enable pipe 0 */
    wr1(REG_SETUP_AW,  0x03);  /* 5-byte address */
    wr1(REG_RF_CH,     channel);
    wr1(REG_RF_SETUP,  0x0E);  /* 2 Mbps, 0 dBm */
    wr1(REG_FEATURE,   0x04);  /* EN_DPL */
    wr1(REG_DYNPD,     0x01);  /* DPL on pipe 0 */
    wr(REG_RX_ADDR_P0, addr, 5);
    wr1(REG_STATUS,    0x70);  /* clear IRQ flags */
    cmd(CMD_FLUSH_RX);
    cmd(CMD_FLUSH_TX);
    hw->ce(true);              /* start listening */
}

uint8_t nrf24_read_payload(uint8_t *buf)
{
    if (rd1(REG_FIFO_STATUS) & FIFO_RX_EMPTY) return 0;

    hw->csn(false);
    hw->spi_xfer(CMD_R_RX_PL_WID);
    uint8_t len = hw->spi_xfer(0xFF);
    hw->csn(true);

    if (len == 0 || len > 32) { cmd(CMD_FLUSH_RX); return 0; } /* datasheet: flush on invalid width */

    hw->csn(false);
    hw->spi_xfer(CMD_R_RX_PAYLOAD);
    for (uint8_t i = 0; i < len; i++) buf[i] = hw->spi_xfer(0xFF);
    hw->csn(true);

    wr1(REG_STATUS, STATUS_RX_DR);
    return len;
}
