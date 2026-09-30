# VERIFY.md - every register value, API name and pin, with its source

Status legend
- **SRC-OK**   matches the named open-source reference (file cited). Not the datasheet.
- **COMPILED** the name/struct resolves in a real build against the pinned version.
- **DERIVED**  computed from SRC-OK values (arithmetic shown).
- **UNVERIFIED** no source was available; taken from memory or assumed. Do not trust.
- **HW-ONLY**  can only be settled on hardware.

Reference versions actually used (shallow clones, 2026-09-29):
RF24 = nRF24/RF24 master; TinyUSB = hathach/tinyusb tag 0.19.0;
NCS = nrfconnect/sdk-nrf v3.4.1 (esb.h byte-identical to main);
HAL = STMicroelectronics/stm32f4xx_hal_driver + cmsis_device_f4 (master);
CubeF4 example = Projects/STM324x9I_EVAL/Applications/USB_Device/HID_Standalone;
pinctrl = zephyrproject-rtos/hal_stm32 dts/st/f4/stm32f405rgtx-pinctrl.dtsi (generated from ST's CubeMX DB).

**No datasheets were reachable** (nordicsemi.com, st.com, microchip.com all blocked
by the sandbox proxy; `docs/` is absent from the repo). Anything that only a
datasheet can settle (timing, electrical, power-up sequence, package pin
availability) is UNVERIFIED below.

## 1. nRF24L01+ (dongle/src/nrf24l01p.c)

| Item | Value in code | Source | Status |
|---|---|---|---|
| R_REGISTER / W_REGISTER | 0x00 / 0x20 | RF24 nRF24L01.h | SRC-OK |
| R_RX_PL_WID | 0x60 | RF24 nRF24L01.h | SRC-OK |
| R_RX_PAYLOAD | 0x61 | RF24 nRF24L01.h | SRC-OK |
| FLUSH_TX / FLUSH_RX | 0xE1 / 0xE2 | RF24 nRF24L01.h | SRC-OK |
| CONFIG, EN_AA, EN_RXADDR, SETUP_AW, RF_CH, RF_SETUP, STATUS | 0x00,0x01,0x02,0x03,0x05,0x06,0x07 | RF24 nRF24L01.h | SRC-OK |
| RX_ADDR_P0, FIFO_STATUS, DYNPD, FEATURE | 0x0A, 0x17, 0x1C, 0x1D | RF24 nRF24L01.h | SRC-OK |
| CONFIG bits EN_CRC=3, CRCO=2, PWR_UP=1, PRIM_RX=0 | 0x0F = all four | RF24 bit mnemonics | SRC-OK / DERIVED (8+4+2+1) |
| RF_SETUP 2 Mbps, 0 dBm | 0x0E | RF_DR_HIGH=bit3 (0x08), RF_DR_LOW=bit5 must be 0, RF_PWR=bits2:1 both set (0x06); RF24 nRF24L01.h + RF24.cpp setDataRate | DERIVED (0x08+0x06) |
| FEATURE EN_DPL | 0x04 (bit 2) | RF24 nRF24L01.h EN_DPL=2 | SRC-OK |
| DYNPD pipe 0 | 0x01 | RF24 DPL_P0=0 | SRC-OK |
| STATUS clear-all mask | 0x70 (RX_DR=6, TX_DS=5, MAX_RT=4) | RF24 | SRC-OK |
| FIFO_STATUS RX_EMPTY | bit 0 | RF24 RX_EMPTY=0 | SRC-OK |
| SETUP_AW = 0x03 (5-byte address) | 0x03 | not re-read in RF24 in this pass | UNVERIFIED (low risk) |
| Flush RX on invalid payload width (0 or >32) | yes | RF24.cpp ~L1608 | SRC-OK |
| Address byte order (LSByte first on SPI) | all-0xE7 so moot | - | moot for this address |
| Power-on wait before first SPI | 100 ms | RF24 uses delay(100) on Linux path, 5 ms elsewhere; datasheet not read | UNVERIFIED (conservative) |
| PWR_UP -> standby before CE high | 5 ms | RF24_POWERUP_DELAY = 5000 us (RF24_config.h) | SRC-OK for RF24; datasheet value UNVERIFIED |
| SPI mode 0, MSB first, max 10 MHz | mode 0, 5.25 MHz | RF24_SPI_SPEED 10000000 (RF24_config.h); SPI mode from memory | speed SRC-OK, mode UNVERIFIED |
| Init read-back (CONFIG, RF_CH) | added in this pass | design choice | HW-ONLY (tested only against a mock) |

Change made after checking: the old init wrote CONFIG (PWR_UP|PRIM_RX) first
with no delay and raised CE immediately. Now registers are written while
powered down, PWR_UP last, then a 5 ms wait, then CE high, plus a read-back.

## 2. ESB, keyboard side (keyboard_esb_addon/esb_tx.c)

Checked in nrfconnect/sdk-nrf `include/esb.h` (v3.4.1) and by building against it.

| Name | Status |
|---|---|
| `esb_init`, `esb_write_payload`, `esb_flush_tx`, `esb_set_base_address_0`, `esb_set_prefixes(const uint8_t*, uint8_t)`, `esb_set_rf_channel(uint32_t)` | SRC-OK + COMPILED |
| `struct esb_config` fields: `protocol, mode, event_handler, bitrate, crc, tx_output_power, retransmit_delay, retransmit_count, tx_mode, payload_length, selective_auto_ack, use_fast_ramp_up` (used: protocol, mode, bitrate, crc, event_handler, retransmit_count, selective_auto_ack) | SRC-OK + COMPILED |
| `struct esb_payload` fields `length, pipe, rssi, noack, pid, data[]` (used: length, pipe, noack, data) | SRC-OK + COMPILED |
| Enums `ESB_PROTOCOL_ESB_DPL, ESB_MODE_PTX, ESB_BITRATE_2MBPS, ESB_CRC_16BIT`, events `ESB_EVENT_TX_SUCCESS/TX_FAILED` | SRC-OK + COMPILED |
| `ESB_DEFAULT_CONFIG` exists (DPL, 2 Mbps, CRC16, retransmit_delay=600, count=3) | SRC-OK |
| `ESB_LEGACY_CONFIG` = `ESB_PROTOCOL_ESB` (fixed payload) + `ESB_CRC_8BIT` | SRC-OK; **not what esb_tx.c uses** |
| Bug found by compiling: local `esb_event_handler` collided with the `esb_event_handler` typedef in esb.h | fixed (renamed `n96_esb_evt_cb`) |
| Kconfig `CONFIG_ESB=y`, `CONFIG_ESB_CLOCK_INIT=y` (starts HFCLK in esb_init) | SRC-OK (subsys/esb/Kconfig, samples/esb/esb_ptx/prj.conf) + COMPILED |
| `esb_flush_tx()` / `esb_write_payload()` are legal from the event handler | handler runs from a software event interrupt with state IDLE (esb.c ~L2235); sample calls flush_tx outside the handler. Plausible, UNVERIFIED on target |
| nRF5-ESB <-> nRF24L01+ on-air compatibility | NCS doc (doc/nrf/protocols/esb/index.rst): "compatible with nRF24L Series"; recipe given is `ESB_LEGACY_CONFIG`. DPL + CRC16 is what the nRF24L01+ hardware supports, but the docs do not say that exact combo was tested. **HW-ONLY** |
| Address: base0 = E7E7E7E7 + prefix E7 = 5x0xE7; doc says ESB rearranges bytes to match nRF24L | all-equal bytes so byte order is moot. HW-ONLY |
| `retransmit_delay` semantics: start-to-start, unlike nRF24 (end-to-start) | SRC-OK (docs). Only matters for PTX; the nRF24 is the PRX |
| Build board used for the compile check | `nrf52840dk/nrf52840`, gnuarmemb (apt gcc 13.2.1), `--no-sysbuild` - the real keyboard board is unknown | COMPILED for that board only |

## 3. TinyUSB (dongle/src/usb_descriptors.c, main.c) - tag 0.19.0

| Name | Where | Status |
|---|---|---|
| `TUD_CONFIG_DESCRIPTOR(config_num, itfcount, stridx, total_len, attribute, power_ma)` | src/device/usbd.h:221 | SRC-OK |
| `TUD_HID_DESCRIPTOR(itfnum, stridx, boot_protocol, report_desc_len, epin, epsize, ep_interval)` | usbd.h:300 | SRC-OK |
| `TUD_CONFIG_DESC_LEN`=9, `TUD_HID_DESC_LEN`=25 | usbd.h:218,296 | SRC-OK |
| `TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP` | src/common/tusb_types.h:255 | SRC-OK |
| `tud_descriptor_device_cb`, `_configuration_cb(uint8_t)`, `_string_cb(uint8_t, uint16_t)` | usbd.h:129-137 | SRC-OK |
| `tud_descriptor_device_qualifier_cb(void)`, `tud_descriptor_other_speed_configuration_cb(uint8_t)` | usbd.h:147,152; weak defaults return NULL (usbd.c:60-67) so a HS device without them STALLs the requests | SRC-OK |
| `tud_hid_descriptor_report_cb`, `tud_hid_get_report_cb(instance, report_id, report_type, buffer, reqlen)`, `tud_hid_set_report_cb(instance, report_id, report_type, buffer, bufsize)` | hid_device.h:130-139 | SRC-OK |
| `tud_hid_ready()`, `tud_hid_report(report_id, report, len)` | hid_device.h:88,100 | SRC-OK |
| `tud_hid_report(0, ...)` sends the buffer as-is (no ID byte) when report_id==0; fails (returns false) if len > `CFG_TUD_HID_EP_BUFSIZE`, so tusb_config.h sets it to 16 for the 14-byte report. hid_device.c hardcodes rhport 0 but usbd_edpt_* substitute `_usbd_rhport`, so rhport 1 works | src/class/hid/hid_device.c:113-132, usbd.c:1382-1414 | SRC-OK |
| `tusb_init()` with no args needs `TUD_OPT_RHPORT`; new style is `tusb_init(rhport, &(tusb_rhport_init_t){.role,.speed})` | tusb.h:142-157, tusb_types.h:321 | SRC-OK |
| STM32F4 OTG_HS = TinyUSB rhport 1; `OTG_HS_IRQHandler` must call `tusb_int_handler(1, true)` | hw/bsp/stm32f4/family.c:45-51 | SRC-OK. **main.c had no IRQ handler** - added in board.c |
| `tusb_time_millis_api()` is `extern` and must be provided by the app | src/common/tusb_common.h:87 | SRC-OK; added in board.c |
| HS enumeration needs `CFG_TUD_MAX_SPEED = OPT_MODE_HIGH_SPEED` (else default) | tusb_option.h:245-357 | SRC-OK |
| dwc2 driver reads PHY type (ULPI) from GHWCFG2 itself; nothing to set for ULPI on F4 | dwc2_common.c:115,150; dwc2_stm32.h:186-260 | SRC-OK (reading) - behaviour HW-ONLY |
| VBUS handling in ULPI mode (B-session valid) on F4 via USB3300 | not investigated | UNVERIFIED |
| HS interrupt endpoint: wMaxPacketSize 16, bInterval=1 => 2^(1-1) = 1 microframe = 125 us | USB 2.0 spec (from memory) | UNVERIFIED (no spec in sandbox); standard and widely known |

## 4. STM32F405 pins (dongle/src/board.c) and the pin plan in main.c

| Signal | Pin / AF | Source | Status |
|---|---|---|---|
| ULPI CK PA5, D0 PA3, D1 PB0, D2 PB1, D3 PB10, D4 PB11, D5 PB12, D6 PB13, D7 PB5, STP PC0 - all AF10 | | CubeF4 HID_Standalone usbd_conf.c (STM324x9I_EVAL, F429) + hal_stm32 F405RGTx pinctrl (`STM32_PINMUX(..., AF10)`) | SRC-OK (two sources; not datasheet) |
| ULPI DIR PC2, NXT PC3 - AF10 | | hal_stm32 F405RGTx pinctrl only. The Cube example uses PI11 / PH4 (176-pin F429 pins) which do not exist on F405 LQFP64 | SRC-OK (one source) |
| `GPIO_AF10_OTG_HS` = 0x0A, `GPIO_AF6_SPI3` = 0x06 | | stm32f4xx_hal_gpio_ex.h | SRC-OK |
| SPI3 SCK PC10, MISO PC11, MOSI PC12 - AF6 | | hal_stm32 F405RGTx pinctrl | SRC-OK |
| PA4 (CSN), PA6 (CE), PA7 (IRQ) as plain GPIO | | PA4 is also SPI3_NSS/I2S3_WS in pinctrl, unused | no conflict found |
| Package: pinctrl file is for STM32F405RGTx (LQFP64); all pins above exist there | | same file | SRC-OK for RGTx. Other packages: not checked |
| Plan conflict check: no two signals share a pin | | manual check of list | DERIVED |
| USB3300 RESET pin, VBUS wiring, reference clock source | not in the pin plan | see SUMMARY.md | **hardware question for you** |
| HSE frequency of the dongle board | unknown | board.c takes `N96_HSE_HZ` at build time | UNVERIFIED assumption |

## 5. STM32F405 clocks, SPI3, memory map (dongle/src/board.c, linker script)

All of these compile against HAL v1.8.5 / CMSIS device v2.6.9 (COMPILED for
names/macros). The *values* are from memory of the F405 family and have not
been checked against the datasheet/reference manual (unreachable).

| Item | Value | Status |
|---|---|---|
| SYSCLK 168 MHz: PLLM = HSE/1 MHz, PLLN 336, PLLP /2, PLLQ 7 | VCO in 1 MHz, VCO 336 MHz | UNVERIFIED (F405 max 168 MHz from memory) |
| AHB /1, APB1 /4 (42 MHz), APB2 /2 (84 MHz) | | UNVERIFIED (APB limits 42/84 MHz from memory) |
| Flash latency 5 wait states at 168 MHz, VOS scale 1 | `FLASH_LATENCY_5`, `PWR_REGULATOR_VOLTAGE_SCALE1` | UNVERIFIED |
| HSE frequency | build-time `N96_HSE_HZ`, default 8 MHz | **assumption** - board unknown |
| ULPI 60 MHz clock comes from the USB3300 on PA5, not the PLL | | UNVERIFIED (standard ULPI behaviour, from memory) |
| SPI3 on APB1, CR1: MSTR, SSM, SSI, BR=0b010 (/8) -> 5.25 MHz, mode 0, MSB first, 8-bit | | register bit names COMPILED; BR encoding UNVERIFIED |
| SPI DR accessed as 8-bit (`*(volatile uint8_t*)&SPI3->DR`) | | UNVERIFIED (common practice, from memory) |
| Clock enable macros `__HAL_RCC_USB_OTG_HS_CLK_ENABLE`, `__HAL_RCC_USB_OTG_HS_ULPI_CLK_ENABLE`, `__HAL_RCC_SPI3_CLK_ENABLE` | | SRC-OK (CubeF4 example uses the first two) + COMPILED |
| `OTG_HS_IRQn`, `OTG_HS_IRQHandler` symbol name in startup file | | COMPILED and confirmed in the linked ELF (`arm-none-eabi-nm`): our handler overrides the weak default |
| `UID_BASE` = 0x1FFF7A10 (12-byte unique ID) | | SRC-OK (cmsis_device_f4 stm32f405xx.h) |
| RAM 128 KiB @ 0x20000000, CCM 64 KiB @ 0x10000000, flash 1 MiB @ 0x08000000 | linker script | UNVERIFIED (memory) |
| Startup file / system_stm32f4xx.c | cmsis_device_f4 Templates (`startup_stm32f405xx.s`) | SRC-OK, used unmodified |
| USB3300 RESET/VBUS handling, ULPI VBUS indicator bits | not implemented | UNVERIFIED / open |

## 6. What the host tests do and do not establish

| Test | Establishes | Does not establish |
|---|---|---|
| test_protocol | packet layout (16 B, field offsets), bounds checking, seq arithmetic | that the keyboard fills it correctly |
| test_report | NKRO descriptor describes exactly 14 report bytes (walked item by item), bit N -> usage N | that Windows/Linux/macOS accept the descriptor |
| test_nrf24 | driver emits the exact transcript listed in the test; each assertion killed a deliberate mutation | that the transcript is right for the chip: expected bytes come from RF24, not the datasheet |
| test_usb_descriptors | descriptor lengths chain, qualifier/other-speed present and consistent, bInterval byte = 1, string encoding | enumeration, HS negotiation |
| test_rate_tool.py | counting/gap/wrap logic of the rate tool | behaviour against the real firmware |

## 7. Second-pass additions (2026-09-30)

`docs/` still contains no datasheets (only TESTING.md and SCHEMATIC_CHECKLIST.md),
so no UNVERIFIED row above could be re-checked against one. Rows below are new.

| Item | Source | Status |
|---|---|---|
| `tud_hid_set_protocol_cb(uint8_t instance, uint8_t protocol)`; protocol is `HID_PROTOCOL_BOOT` (0) or `HID_PROTOCOL_REPORT` (1) | hid_device.h:142-143, hid.h:147-148 | SRC-OK + COMPILED (static-asserted against `N96_PROTO_*` in main.c) |
| Interface protocol byte: `TUD_HID_DESCRIPTOR(..., HID_ITF_PROTOCOL_KEYBOARD, ...)` also sets bInterfaceSubClass=1 (boot) | usbd.h:300-302 | SRC-OK; checked by test_usb_descriptors |
| TinyUSB starts each interface in **report** protocol ("Per Specs: default is report mode"); SET_PROTOCOL sets `protocol_mode` at the ACK stage and calls the callback; GET_PROTOCOL answers it | hid_device.c:258, 361-373 | SRC-OK |
| SET_IDLE: `tud_hid_set_idle_cb` weak default returns true; idle rate stored, not honoured by us | hid_device.c:79-88, 346-351 | SRC-OK |
| Boot keyboard report = 8 bytes: modifiers, reserved, 6 keycodes; ErrorRollOver = usage 0x01 in all six slots when more than six keys are down | HID spec / usage tables from memory | **UNVERIFIED** (spec not available) |
| BIOS/UEFI actually enumerating and typing with boot protocol | - | **HW-ONLY** |
| Windows/Linux accept an NKRO report descriptor on a boot-subclass interface (QMK-style dual mode) | from memory of common practice | **HW-ONLY** |
| TinyUSB 0.19.0 dwc2 forces B-session valid (`GOTGCTL.BVALOEN\|BVALOVAL`), sets `DCFG.XCVRDLY` for ULPI PHYs, clears `ULPIEVBUSD/I` | dcd_dwc2.c:~405-426, dwc2_common.c:~127 | SRC-OK (reading); behaviour with the USB3300 HW-ONLY |
| `tud_connect()` / `tud_disconnect()` exist | usbd.h:106-110 | SRC-OK |
| `OTG_HS_VBUS` = PB13, `OTG_HS_ID` = PB12 (collide with ULPI D6/D5); `OTG_HS_SOF` = PA4; `OTG_FS` DM/DP/VBUS/ID = PA11/PA12/PA9/PA10; SWDIO PA13, SWCLK PA14 | hal_stm32 F405RGTx pinctrl | SRC-1 |
| `k_uptime_get_32()` used for resend timing on the keyboard | Zephyr | COMPILED |
| Stuck-key constants: resend 20 ms, dongle timeout 100 ms | our choice | **UNMEASURED guesses** (DECISIONS.md section 6) |
| STM32 ROM USB-DFU uses OTG_FS (PA11/PA12) only; BOOT0/BOOT1(PB2) behaviour; HSE range 4-26 MHz; USB3300 pin functions and polarities | memory (ST AN2606 / datasheets unreachable) | **UNVERIFIED** - listed as MEM in SCHEMATIC_CHECKLIST.md |
| nRF24 IRQ is active-low, module 8-pin order | memory | **UNVERIFIED** |

New host tests (all pass; each key assertion killed at least one deliberate mutation):
test_keepalive (resend pacing, retry after TX failure, wrap-safe timing, timeout predicate),
test_core (payload -> state -> report, hold timeout, resends keep a key alive for 5 s of
simulated time, lost-release recovery both ways, boot/report protocol switch, timeout in
boot protocol), boot-report cases in test_report, boot subclass/protocol bytes in
test_usb_descriptors. They show the logic; they do not show RF or USB behaviour.
