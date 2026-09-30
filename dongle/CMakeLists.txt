cmake_minimum_required(VERSION 3.20)

# Usage (after tools/fetch_deps.sh):
#   cmake -S dongle -B build/dongle -DCMAKE_TOOLCHAIN_FILE=dongle/cmake/arm-none-eabi.cmake \
#         [-DN96_HSE_HZ=8000000] [-DN96_TEST_MODE=ON]
#   cmake --build build/dongle
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake CACHE FILEPATH "")
project(n96_dongle C ASM)

set(N96_HSE_HZ 8000000 CACHE STRING "HSE crystal frequency in Hz (UNKNOWN for this board)")
option(N96_TEST_MODE "Build the USB rate-test firmware (no radio, vendor report)" OFF)

set(TP ${CMAKE_CURRENT_SOURCE_DIR}/../third_party)
set(TUSB ${TP}/tinyusb/src)
set(HAL  ${TP}/stm32f4xx_hal_driver)
set(DEV  ${TP}/cmsis_device_f4)
set(CORE ${TP}/cmsis_core)
foreach(d ${TUSB} ${HAL} ${DEV} ${CORE})
  if(NOT EXISTS ${d})
    message(FATAL_ERROR "${d} missing - run tools/fetch_deps.sh")
  endif()
endforeach()

set(CMAKE_C_STANDARD 11)
set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)

set(APP_SRC
  src/main.c src/board.c src/nrf24l01p.c src/report.c src/core.c src/usb_descriptors.c)
set(TUSB_SRC
  ${TUSB}/tusb.c ${TUSB}/common/tusb_fifo.c
  ${TUSB}/device/usbd.c ${TUSB}/device/usbd_control.c
  ${TUSB}/class/hid/hid_device.c
  ${TUSB}/portable/synopsys/dwc2/dcd_dwc2.c ${TUSB}/portable/synopsys/dwc2/dwc2_common.c)
set(HAL_SRC
  ${HAL}/Src/stm32f4xx_hal.c ${HAL}/Src/stm32f4xx_hal_rcc.c ${HAL}/Src/stm32f4xx_hal_rcc_ex.c
  ${HAL}/Src/stm32f4xx_hal_gpio.c ${HAL}/Src/stm32f4xx_hal_cortex.c
  ${HAL}/Src/stm32f4xx_hal_pwr.c ${HAL}/Src/stm32f4xx_hal_pwr_ex.c
  ${HAL}/Src/stm32f4xx_hal_flash.c ${HAL}/Src/stm32f4xx_hal_flash_ex.c)
set(SYS_SRC
  ${DEV}/Source/Templates/system_stm32f4xx.c
  ${DEV}/Source/Templates/gcc/startup_stm32f405xx.s)

add_executable(n96_dongle ${APP_SRC} ${TUSB_SRC} ${HAL_SRC} ${SYS_SRC})

target_include_directories(n96_dongle PRIVATE
  src ../shared ${TUSB}
  ${HAL}/Inc ${DEV}/Include ${CORE}/Include)

target_compile_definitions(n96_dongle PRIVATE
  STM32F405xx USE_HAL_DRIVER HSE_VALUE=${N96_HSE_HZ} N96_HSE_HZ=${N96_HSE_HZ}
  CFG_TUSB_MCU=OPT_MCU_STM32F4)
if(N96_TEST_MODE)
  target_compile_definitions(n96_dongle PRIVATE N96_TEST_MODE=1)
endif()

# Warnings only on our own sources; vendor code stays quiet.
set_source_files_properties(${APP_SRC} PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror")
target_compile_options(n96_dongle PRIVATE ${CPU_FLAGS} -Os -g3 -ffunction-sections -fdata-sections)
target_link_options(n96_dongle PRIVATE ${CPU_FLAGS} --specs=nano.specs
  -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F405RGTx_FLASH.ld
  -Wl,--gc-sections -Wl,-Map=n96_dongle.map)

add_custom_command(TARGET n96_dongle POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:n96_dongle>
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:n96_dongle> n96_dongle.bin)
