/*
 * nRF24 driver tests against a mock SPI. The mock is a tiny model of the
 * chip's SPI command interface (register file, RX FIFO) that ALSO records a
 * transcript of everything the driver does. The tests assert the exact
 * transcript, so any change to the register write sequence fails loudly.
 *
 * What this proves: the driver emits the sequence written down here.
 * What it does NOT prove: that the sequence is right for the silicon. The
 * expected bytes come from the RF24 header (VERIFY.md), not the datasheet,
 * and nothing here has touched a real nRF24L01+.
 */
#include "check.h"
#include "nrf24l01p.h"

#define LOG_MAX 8192
static char g_log[LOG_MAX];
static size_t g_len;

static void logf_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#include <stdarg.h>
static void logf_(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    g_len += (size_t)vsnprintf(g_log + g_len, LOG_MAX - g_len, fmt, ap);
    va_end(ap);
}

/* --- chip model --- */
static uint8_t reg[0x20];
static struct { uint8_t len; uint8_t data[32]; } rxq[3];
static int rxq_n;
static bool csn_level, ce_level;
static int txn_pos;              /* byte index within the current transaction */
static uint8_t txn_cmd;
static char txn_buf[128];
static size_t txn_len;
static uint8_t last_pop[32];
static int fail_reads;           /* when >0, register reads return 0x00 (dead module) */

static void model_reset(void)
{
    memset(reg, 0, sizeof reg);
    reg[0x00] = 0x08; reg[0x01] = 0x3F; reg[0x02] = 0x03; reg[0x03] = 0x03;
    reg[0x05] = 0x02; reg[0x06] = 0x0E; reg[0x07] = 0x0E;
    rxq_n = 0; csn_level = true; ce_level = false; txn_pos = 0; g_len = 0; g_log[0] = 0;
    fail_reads = 0;
}

static void flush_txn(void)
{
    if (txn_len) { txn_buf[txn_len] = 0; logf_("SPI %s\n", txn_buf); }
    txn_len = 0; txn_pos = 0;
}

static uint8_t m_spi(uint8_t tx)
{
    uint8_t rx = reg[0x07];                 /* first byte clocked out is always STATUS */
    if (csn_level) { logf_("!! SPI byte while CSN high\n"); return 0; }
    if (txn_pos == 0) {
        txn_cmd = tx;
    } else {
        uint8_t c = txn_cmd;
        if ((c & 0xE0) == 0x00) {           /* R_REGISTER */
            uint8_t r = c & 0x1F;
            rx = fail_reads ? 0x00 : reg[r];
        } else if ((c & 0xE0) == 0x20) {    /* W_REGISTER */
            uint8_t r = c & 0x1F;
            if (r == 0x07) reg[r] &= (uint8_t)~(tx & 0x70);     /* write-1-to-clear IRQ flags */
            else reg[r] = tx;
        } else if (c == 0x60) {             /* R_RX_PL_WID */
            rx = rxq_n ? rxq[0].len : 0;
        } else if (c == 0x61) {             /* R_RX_PAYLOAD */
            rx = rxq_n ? rxq[0].data[txn_pos - 1] : 0;
            last_pop[txn_pos - 1] = rx;
        }
    }
    txn_len += (size_t)snprintf(txn_buf + txn_len, sizeof txn_buf - txn_len, txn_pos ? " %02X" : "%02X", tx);
    txn_pos++;
    return rx;
}

static void m_csn(bool level)
{
    if (!level && csn_level) { txn_pos = 0; txn_len = 0; }
    if (level && !csn_level) {
        /* transaction complete: apply side effects that happen on CSN rising edge */
        if (txn_cmd == 0xE2) rxq_n = 0;
        if (txn_cmd == 0x61 && rxq_n) { memmove(&rxq[0], &rxq[1], sizeof rxq[0] * (size_t)(--rxq_n)); }
        flush_txn();
    }
    csn_level = level;
}
static void m_ce(bool level) { ce_level = level; logf_("CE %d\n", level); }
static void m_delay(uint32_t ms) { logf_("DELAY %ums\n", (unsigned)ms); }

/* FIFO_STATUS is derived, not stored */
static uint8_t fifo_status(void) { return rxq_n ? 0x00 : 0x01; }
static uint8_t m_spi_wrap(uint8_t tx)
{
    /* intercept FIFO_STATUS reads so the model reflects the queue */
    reg[0x17] = fifo_status();
    return m_spi(tx);
}

static const nrf24_hw_t hw = { m_spi_wrap, m_csn, m_ce, m_delay };
static const uint8_t addr[5] = { 0xE7, 0xE7, 0xE7, 0xE7, 0xE7 };

static void queue_payload(const uint8_t *d, uint8_t n)
{
    rxq[rxq_n].len = n; memcpy(rxq[rxq_n].data, d, n); rxq_n++;
}

static void test_init_sequence(void)
{
    model_reset();
    CHECK(nrf24_init_prx(&hw, 76, addr));
    const char *expected =
        "DELAY 100ms\n"
        "SPI 21 01\n"                       /* EN_AA: auto-ack pipe 0 */
        "SPI 22 01\n"                       /* EN_RXADDR: pipe 0 */
        "SPI 23 03\n"                       /* SETUP_AW: 5 bytes */
        "SPI 25 4C\n"                       /* RF_CH = 76 */
        "SPI 26 0E\n"                       /* RF_SETUP: 2 Mbps, 0 dBm */
        "SPI 3D 04\n"                       /* FEATURE: EN_DPL */
        "SPI 3C 01\n"                       /* DYNPD: pipe 0 */
        "SPI 2A E7 E7 E7 E7 E7\n"           /* RX_ADDR_P0 */
        "SPI 27 70\n"                       /* STATUS: clear IRQ flags */
        "SPI E2\n"                          /* FLUSH_RX */
        "SPI E1\n"                          /* FLUSH_TX */
        "SPI 20 0F\n"                       /* CONFIG: EN_CRC|CRCO|PWR_UP|PRIM_RX */
        "DELAY 5ms\n"
        "SPI 00 FF\n"                       /* read back CONFIG */
        "SPI 05 FF\n"                       /* read back RF_CH */
        "CE 1\n";
    /* CE must have been driven low before anything else; the model logs CE only via the hook,
     * so the very first log entry being the 100 ms delay means CE(0) came first (see below). */
    const char *first_ce0 = "CE 0\n";
    CHECK(strncmp(g_log, first_ce0, strlen(first_ce0)) == 0);
    CHECK_STR(g_log + strlen(first_ce0), expected);
    CHECK_EQ(reg[0x00], 0x0F);
    CHECK_EQ(reg[0x05], 76);
    CHECK(ce_level);
}

static void test_init_reports_dead_module(void)
{
    model_reset();
    fail_reads = 1;                          /* SPI reads come back 0x00: module absent/miswired */
    CHECK(!nrf24_init_prx(&hw, 76, addr));
    CHECK(ce_level);                         /* documented: CE is still raised; caller decides */
}

static void test_power_up_ordering(void)
{
    /* PWR_UP write must come after all config writes and be followed by a delay before CE high */
    model_reset();
    nrf24_init_prx(&hw, 10, addr);
    const char *pwr = strstr(g_log, "SPI 20 0F\n");
    const char *ce1 = strstr(g_log, "CE 1\n");
    const char *dly = strstr(pwr, "DELAY 5ms\n");
    CHECK(pwr && ce1 && dly);
    CHECK(pwr < dly && dly < ce1);
    CHECK(strstr(g_log, "SPI 25 0A\n") != NULL);   /* channel parameter is honoured */
}

static void test_read_empty(void)
{
    model_reset(); nrf24_init_prx(&hw, 76, addr); g_len = 0; g_log[0] = 0;
    uint8_t buf[32];
    CHECK_EQ(nrf24_read_payload(buf), 0);
    CHECK_STR(g_log, "SPI 17 FF\n");         /* only a FIFO_STATUS read, nothing else */
}

static void test_read_one_payload(void)
{
    model_reset(); nrf24_init_prx(&hw, 76, addr);
    uint8_t p[16]; for (int i = 0; i < 16; i++) p[i] = (uint8_t)(0x30 + i);
    queue_payload(p, 16);
    g_len = 0; g_log[0] = 0;
    uint8_t buf[32] = {0};
    CHECK_EQ(nrf24_read_payload(buf), 16);
    CHECK(memcmp(buf, p, 16) == 0);
    CHECK_STR(g_log,
        "SPI 17 FF\n"                        /* FIFO_STATUS */
        "SPI 60 FF\n"                        /* R_RX_PL_WID */
        "SPI 61 FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF\n"   /* R_RX_PAYLOAD, 16 bytes */
        "SPI 27 40\n");                      /* clear RX_DR */
    CHECK_EQ(nrf24_read_payload(buf), 0);    /* drained */
}

static void test_read_two_payloads(void)
{
    model_reset(); nrf24_init_prx(&hw, 76, addr);
    uint8_t a[3] = {1,2,3}, b[5] = {9,8,7,6,5};
    queue_payload(a, 3); queue_payload(b, 5);
    uint8_t buf[32];
    CHECK_EQ(nrf24_read_payload(buf), 3); CHECK(memcmp(buf, a, 3) == 0);
    CHECK_EQ(nrf24_read_payload(buf), 5); CHECK(memcmp(buf, b, 5) == 0);
    CHECK_EQ(nrf24_read_payload(buf), 0);
}

static void test_invalid_width_flushes(void)
{
    model_reset(); nrf24_init_prx(&hw, 76, addr);
    uint8_t junk[33] = {0};
    queue_payload(junk, 33);                 /* width > 32: corrupt */
    g_len = 0; g_log[0] = 0;
    uint8_t buf[32];
    CHECK_EQ(nrf24_read_payload(buf), 0);
    CHECK_STR(g_log, "SPI 17 FF\nSPI 60 FF\nSPI E2\n");
    CHECK_EQ(rxq_n, 0);

    queue_payload(junk, 0);                  /* width 0: also invalid */
    g_len = 0; g_log[0] = 0;
    CHECK_EQ(nrf24_read_payload(buf), 0);
    CHECK_STR(g_log, "SPI 17 FF\nSPI 60 FF\nSPI E2\n");
}

static void test_max_width(void)
{
    model_reset(); nrf24_init_prx(&hw, 76, addr);
    uint8_t p[32]; for (int i = 0; i < 32; i++) p[i] = (uint8_t)i;
    queue_payload(p, 32);
    uint8_t buf[32];
    CHECK_EQ(nrf24_read_payload(buf), 32); CHECK(memcmp(buf, p, 32) == 0);
}

static void test_no_spi_with_csn_high(void)
{
    model_reset(); nrf24_init_prx(&hw, 76, addr);
    uint8_t p[4] = {1,2,3,4}; queue_payload(p, 4);
    uint8_t buf[32]; nrf24_read_payload(buf);
    CHECK(strstr(g_log, "!!") == NULL);
}

int main(void)
{
    test_init_sequence(); test_init_reports_dead_module(); test_power_up_ordering();
    test_read_empty(); test_read_one_payload(); test_read_two_payloads();
    test_invalid_width_flushes(); test_max_width(); test_no_spi_with_csn_high();
    TEST_MAIN_END("test_nrf24");
}
