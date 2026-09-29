/*
 * Minimal nRF24L01+ driver (primary receiver, dynamic payloads, 2 Mbps,
 * 16-bit CRC, auto-ack on pipe 0) to match the keyboard's ESB_PROTOCOL_ESB_DPL.
 *
 * Register/command values were cross-checked against nRF24/RF24
 * nRF24L01.h (see VERIFY.md). The datasheet itself was not available, so
 * timing values are marked there as unverified.
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

#define CONFIG_EN_CRC     (1u << 3)
#define CONFIG_CRCO       (1u << 2)
#define CONFIG_PWR_UP     (1u << 1)
#define CONFIG_PRIM_RX    (1u << 0)
#define RF_SETUP_2MBPS_0DBM 0x0E   /* RF_DR_HIGH(bit3) | RF_PWR=0b11 (bits 2:1) */
#define FEATURE_EN_DPL    (1u << 2)

#define STATUS_RX_DR      0x40
#define STATUS_IRQ_ALL    0x70
#define FIFO_RX_EMPTY     0x01

/* Delays are RF24's conservative values, not datasheet-verified. */
#define POWER_ON_MS       100   /* wait after supply is up, before first SPI access */
#define PWR_UP_TO_STANDBY_MS 5  /* RF24 uses 5 ms for Tpd2stby (4.5 ms worst case) */

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

bool nrf24_init_prx(const nrf24_hw_t *h, uint8_t channel, const uint8_t addr[5])
{
    hw = h;
    hw->ce(false);
    hw->csn(true);
    hw->delay_ms(POWER_ON_MS);

    /* Registers are configured while still powered down (SPI works in
     * power-down); PWR_UP goes last so CE is only raised once in standby. */
    wr1(REG_EN_AA,     0x01);  /* auto-ack pipe 0 */
    wr1(REG_EN_RXADDR, 0x01);  /* enable pipe 0 */
    wr1(REG_SETUP_AW,  0x03);  /* 5-byte address */
    wr1(REG_RF_CH,     channel);
    wr1(REG_RF_SETUP,  RF_SETUP_2MBPS_0DBM);
    wr1(REG_FEATURE,   FEATURE_EN_DPL);
    wr1(REG_DYNPD,     0x01);  /* DPL on pipe 0 */
    wr(REG_RX_ADDR_P0, addr, 5);
    wr1(REG_STATUS,    STATUS_IRQ_ALL);
    cmd(CMD_FLUSH_RX);
    cmd(CMD_FLUSH_TX);

    const uint8_t config = CONFIG_EN_CRC | CONFIG_CRCO | CONFIG_PWR_UP | CONFIG_PRIM_RX;
    wr1(REG_CONFIG, config);
    hw->delay_ms(PWR_UP_TO_STANDBY_MS);

    bool ok = rd1(REG_CONFIG) == config && rd1(REG_RF_CH) == channel;
    hw->ce(true);              /* start listening */
    return ok;
}

uint8_t nrf24_read_payload(uint8_t *buf)
{
    if (rd1(REG_FIFO_STATUS) & FIFO_RX_EMPTY) return 0;

    hw->csn(false);
    hw->spi_xfer(CMD_R_RX_PL_WID);
    uint8_t len = hw->spi_xfer(0xFF);
    hw->csn(true);

    if (len == 0 || len > 32) { cmd(CMD_FLUSH_RX); return 0; } /* RF24 flushes on invalid width too */

    hw->csn(false);
    hw->spi_xfer(CMD_R_RX_PAYLOAD);
    for (uint8_t i = 0; i < len; i++) buf[i] = hw->spi_xfer(0xFF);
    hw->csn(true);

    wr1(REG_STATUS, STATUS_RX_DR);
    return len;
}
