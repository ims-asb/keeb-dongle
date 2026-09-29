/* Minimal HAL configuration: only clocks, GPIO and SysTick/NVIC are used.
 * SPI3 is driven with direct register access in board.c. */
#ifndef STM32F4xx_HAL_CONF_H
#define STM32F4xx_HAL_CONF_H

#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED

/* HSE frequency of the dongle board. UNKNOWN - set with -DN96_HSE_HZ=... */
#ifndef N96_HSE_HZ
#define N96_HSE_HZ 8000000U
#endif
#if !defined(HSE_VALUE)
#define HSE_VALUE               N96_HSE_HZ
#endif
#define HSE_STARTUP_TIMEOUT     100U
#define HSI_VALUE               16000000U
#define LSI_VALUE               32000U
#define LSE_VALUE               32768U
#define LSE_STARTUP_TIMEOUT     5000U
#define EXTERNAL_CLOCK_VALUE    12288000U

#define VDD_VALUE               3300U
#define TICK_INT_PRIORITY       0U
#define USE_RTOS                0U
#define PREFETCH_ENABLE         1U
#define INSTRUCTION_CACHE_ENABLE 1U
#define DATA_CACHE_ENABLE       1U
#define USE_HAL_DRIVER_ASSERT   0

#ifdef HAL_RCC_MODULE_ENABLED
#include "stm32f4xx_hal_rcc.h"
#endif
#ifdef HAL_GPIO_MODULE_ENABLED
#include "stm32f4xx_hal_gpio.h"
#endif
#ifdef HAL_CORTEX_MODULE_ENABLED
#include "stm32f4xx_hal_cortex.h"
#endif
#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f4xx_hal_flash.h"
#endif
#ifdef HAL_PWR_MODULE_ENABLED
#include "stm32f4xx_hal_pwr.h"
#endif

#define assert_param(expr) ((void)0U)

#endif
