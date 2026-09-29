/*
 * STM32F405 dongle board support: clocks, ULPI pins (AF10), SPI3 for the
 * nRF24L01+, GPIO, SysTick, OTG_HS interrupt forwarding to TinyUSB.
 *
 * Pin plan (from main.c; sources in VERIFY.md section 4):
 *   ULPI (AF10): CK=PA5 D0=PA3 D1=PB0 D2=PB1 D3=PB10 D4=PB11 D5=PB12
 *                D6=PB13 D7=PB5 STP=PC0 DIR=PC2 NXT=PC3
 *   nRF24 on SPI3 (AF6): SCK=PC10 MISO=PC11 MOSI=PC12
 *                CSN=PA4 CE=PA6 IRQ=PA7 (IRQ configured as input, unused: we poll)
 *
 * NOT written / unknown: USB3300 RESET line, VBUS sensing, HSE frequency
 * (N96_HSE_HZ, default 8 MHz in stm32f4xx_hal_conf.h). Compile-checked only.
 */
#include "stm32f4xx_hal.h"
#include "tusb.h"
#include "board.h"

#if (N96_HSE_HZ % 1000000U) != 0 || (N96_HSE_HZ / 1000000U) < 2 || (N96_HSE_HZ / 1000000U) > 63
#error "N96_HSE_HZ must be an integer number of MHz, 2..63 (PLLM = HSE/1MHz)"
#endif

/* ------------------------------------------------------------------ */
/* Interrupts                                                          */

void SysTick_Handler(void) { HAL_IncTick(); }

/* STM32F4: OTG_HS is TinyUSB rhport 1 (hw/bsp/stm32f4/family.c). */
void OTG_HS_IRQHandler(void) { tusb_int_handler(1, true); }

/* extern in tusb_common.h; required by TinyUSB when running without an OS. */
uint32_t tusb_time_millis_api(void) { return HAL_GetTick(); }

uint32_t board_millis(void) { return HAL_GetTick(); }

void board_delay_ms(uint32_t ms)
{
    uint32_t t0 = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - t0) < ms) { }
}

/* ------------------------------------------------------------------ */
/* Clocks: HSE -> PLL -> 168 MHz SYSCLK, AHB 168, APB1 42, APB2 84.    */
/* The 480 Mbit/s ULPI link is clocked by the USB3300's 60 MHz output  */
/* on PA5, not by these PLLs.                                          */

static void clock_init(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState       = RCC_HSE_ON;
    osc.PLL.PLLState   = RCC_PLL_ON;
    osc.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLM       = N96_HSE_HZ / 1000000U;  /* 1 MHz VCO input */
    osc.PLL.PLLN       = 336;                    /* VCO 336 MHz */
    osc.PLL.PLLP       = RCC_PLLP_DIV2;          /* 168 MHz */
    osc.PLL.PLLQ       = 7;                      /* 48 MHz (unused) */
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) { for (;;) { } }

    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV4;
    clk.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_5) != HAL_OK) { for (;;) { } }

    SystemCoreClockUpdate();
}

/* ------------------------------------------------------------------ */
/* GPIO                                                                */

#define NRF_CSN_PORT GPIOA
#define NRF_CSN_PIN  GPIO_PIN_4
#define NRF_CE_PORT  GPIOA
#define NRF_CE_PIN   GPIO_PIN_6
#define NRF_IRQ_PORT GPIOA
#define NRF_IRQ_PIN  GPIO_PIN_7

static void af_pins(GPIO_TypeDef *port, uint32_t pins, uint32_t af, uint32_t pull)
{
    GPIO_InitTypeDef g = {0};
    g.Pin       = pins;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = pull;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = af;
    HAL_GPIO_Init(port, &g);
}

static void gpio_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* ULPI, all AF10 (GPIO_AF10_OTG_HS). */
    af_pins(GPIOA, GPIO_PIN_5 | GPIO_PIN_3,                       GPIO_AF10_OTG_HS, GPIO_NOPULL); /* CK, D0 */
    af_pins(GPIOB, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_5 |
                   GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 |
                   GPIO_PIN_13,                                    GPIO_AF10_OTG_HS, GPIO_NOPULL); /* D1-D7 */
    af_pins(GPIOC, GPIO_PIN_0 | GPIO_PIN_2 | GPIO_PIN_3,          GPIO_AF10_OTG_HS, GPIO_NOPULL); /* STP, DIR, NXT */

    /* SPI3 (AF6): SCK=PC10 MISO=PC11 MOSI=PC12. */
    af_pins(GPIOC, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12,       GPIO_AF6_SPI3, GPIO_NOPULL);

    /* nRF24 control: CSN idle high, CE idle low, IRQ input. */
    GPIO_InitTypeDef g = {0};
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_WritePin(NRF_CSN_PORT, NRF_CSN_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(NRF_CE_PORT,  NRF_CE_PIN,  GPIO_PIN_RESET);
    g.Pin = NRF_CSN_PIN | NRF_CE_PIN;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = NRF_IRQ_PIN; g.Mode = GPIO_MODE_INPUT; g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(NRF_IRQ_PORT, &g);
}

/* ------------------------------------------------------------------ */
/* SPI3: master, mode 0 (CPOL=0 CPHA=0), 8-bit, MSB first, software NSS. */
/* APB1 = 42 MHz, /8 = 5.25 MHz (nRF24L01+ SPI limit taken as 10 MHz). */

static void spi3_init(void)
{
    __HAL_RCC_SPI3_CLK_ENABLE();
    SPI3->CR1 = 0;
    SPI3->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI | (2u << SPI_CR1_BR_Pos); /* BR=2 -> /8 */
    SPI3->CR1 |= SPI_CR1_SPE;
}

uint8_t board_spi3_xfer(uint8_t b)
{
    while (!(SPI3->SR & SPI_SR_TXE)) { }
    *(volatile uint8_t *)&SPI3->DR = b;
    while (!(SPI3->SR & SPI_SR_RXNE)) { }
    return *(volatile uint8_t *)&SPI3->DR;
}

void board_nrf_csn(bool level)
{
    if (level) {
        while (SPI3->SR & SPI_SR_BSY) { }   /* let the last byte finish before deselect */
    }
    HAL_GPIO_WritePin(NRF_CSN_PORT, NRF_CSN_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void board_nrf_ce(bool level)
{
    HAL_GPIO_WritePin(NRF_CE_PORT, NRF_CE_PIN, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ------------------------------------------------------------------ */

void board_uid_hex(char out[25])
{
    static const char hex[] = "0123456789ABCDEF";
    const uint8_t *uid = (const uint8_t *)UID_BASE;   /* 12 bytes */
    for (int i = 0; i < 12; i++) {
        out[2 * i]     = hex[uid[i] >> 4];
        out[2 * i + 1] = hex[uid[i] & 0xF];
    }
    out[24] = '\0';
}

void board_init(void)
{
    HAL_Init();
    clock_init();
    gpio_init();
    spi3_init();

    /* USB OTG_HS core + ULPI interface clocks (order as in the CubeF4
     * HID_Standalone example: pins first, then ULPI clock, then OTG_HS). */
    __HAL_RCC_USB_OTG_HS_ULPI_CLK_ENABLE();
    __HAL_RCC_USB_OTG_HS_CLK_ENABLE();

    HAL_NVIC_SetPriority(OTG_HS_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(OTG_HS_IRQn);
    /* TinyUSB itself is initialised in main() via tusb_init(). */
}
