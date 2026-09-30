# Schematic checklist - what the firmware assumes about the hardware

Use this to check the KiCad schematic. Direction is from the **MCU's**
point of view. "Assumed in" names where the firmware depends on it.

Status legend
- **SRC-2** two independent open-source references agree (VERIFY.md section 4).
- **SRC-1** one reference (usually ST's CubeMX-derived pinctrl data in Zephyr's hal_stm32).
- **MEM** written from memory of the part's datasheet. **No datasheet was available**
  (`docs/` has none), so every MEM row must be checked by you against the real datasheet.
- **OPEN** the firmware does not decide this; it is a schematic decision. See DECISIONS.md.

## 1. STM32F405 (dongle) <-> USB3300 ULPI PHY

All on alternate function 10 (`GPIO_AF10_OTG_HS`), speed VERY_HIGH, no pull.
Assumed in: `dongle/src/board.c` (`gpio_init()`), `dongle/src/tusb_config.h` (OTG_HS = rhport 1).

| Signal | MCU pin | Direction | Status | Note |
|---|---|---|---|---|
| ULPI CK (60 MHz from PHY) | PA5 | in | SRC-2 | PHY is the clock **source**; the MCU needs this clock running before the OTG_HS core is enabled (MEM) |
| ULPI D0 | PA3 | in/out | SRC-2 | |
| ULPI D1 | PB0 | in/out | SRC-2 | |
| ULPI D2 | PB1 | in/out | SRC-2 | |
| ULPI D3 | PB10 | in/out | SRC-2 | |
| ULPI D4 | PB11 | in/out | SRC-2 | |
| ULPI D5 | PB12 | in/out | SRC-2 | PB12 is also `OTG_HS_ID` in FS-PHY mode; unusable as ID here |
| ULPI D6 | PB13 | in/out | SRC-2 | PB13 is also `OTG_HS_VBUS`; **no MCU VBUS-sense on PB13** in this design (Cube example uses PB13 for VBUS only in its FS-PHY variant) |
| ULPI D7 | PB5 | in/out | SRC-2 | |
| ULPI STP | PC0 | out | SRC-2 | |
| ULPI DIR | PC2 | in | SRC-1 | Cube example uses PI11 (F429 176-pin part); PC2 from pinctrl data for F405RGTx |
| ULPI NXT | PC3 | in | SRC-1 | Cube example uses PH4; PC3 from pinctrl data for F405RGTx |

Check in the schematic: each MCU pin goes to the PHY pin of the **same ULPI name**
(DATA0-7, CLKOUT, DIR, NXT, STP), PHY in **ULPI mode with 8-bit data** (MEM: the
USB3300 has no other interface), no other device on these nets, and the package is
an F405 variant that brings all of these out (pin list checked only for the
LQFP64 STM32F405RGTx pinctrl file; other packages: not checked).

## 2. STM32F405 <-> nRF24L01+ module (SPI3)

Assumed in: `dongle/src/board.c` (`gpio_init()`, `spi3_init()`, `board_spi3_xfer()`,
`board_nrf_csn/ce()`), `dongle/src/main.c` (channel, address).

| Signal | MCU pin | Direction | Config | Status | Note |
|---|---|---|---|---|---|
| SPI3 SCK | PC10 | out | AF6 | SRC-1 | SPI mode 0 (CPOL 0, CPHA 0), MSB first, 5.25 MHz (APB1/8) |
| SPI3 MISO | PC11 | in | AF6 | SRC-1 | |
| SPI3 MOSI | PC12 | out | AF6 | SRC-1 | |
| nRF24 CSN | PA4 | out | GPIO push-pull, idle **high** | n/a (plain GPIO) | Software chip select. PA4 is also `SPI3_NSS`/`OTG_HS_SOF` in pinctrl data; firmware uses it as GPIO only |
| nRF24 CE | PA6 | out | GPIO push-pull, idle **low** | n/a | Driven high by the driver after PWR_UP + 5 ms |
| nRF24 IRQ | PA7 | in | GPIO input, pull-up | n/a | **Wired but not used by firmware** (polling). Active-low on the module (MEM). See DECISIONS.md section 5 |

Check in the schematic:
- Module pin order/labels (standard 8-pin module: GND, VCC, CE, CSN, SCK, MOSI, MISO, IRQ - MEM) matches your footprint and the table above.
- Module supply is within its rated range and its logic is 3.3 V-compatible with the MCU (MEM: 1.9-3.6 V).
- Decoupling at the module (datasheet recommendation, MEM); a module with PA/LNA needs its own supply care.
- No pull-down on CSN (firmware needs it idle high before GPIO init; a pull-up to 3.3 V is the safe default).
- Radio contract (not schematic, but must match the keyboard): channel 76, address E7 E7 E7 E7 E7, 2 Mbps, 16-bit CRC, dynamic payload, auto-ack on pipe 0.

## 3. STM32F405 clocks

| Item | Assumption | Assumed in | Status |
|---|---|---|---|
| HSE crystal/oscillator on OSC_IN/OSC_OUT (PH0/PH1 on the F405) | Present, **integer MHz** (PLLM = HSE / 1 MHz), value passed as `-DN96_HSE_HZ`; default 8 MHz is an assumption | `board.c` `clock_init()`, `stm32f4xx_hal_conf.h`, `dongle/CMakeLists.txt` | **OPEN** - tell the build the real value. Build rejects non-integer MHz or outside 2..63 MHz; the part's own HSE range (4-26 MHz, MEM) is stricter |
| HSE bypass (external clock, not a crystal) | Not supported (`RCC_HSE_ON`) | `board.c` | n/a |
| Load capacitors / crystal spec | Not a firmware matter | - | check against datasheet |
| The USB3300's own reference (crystal or clock input) | Separate from the MCU HSE; the MCU never uses it | - | MEM: USB3300 is normally driven from a 24 MHz crystal - check datasheet |
| LSE (32.768 kHz) | Not used | - | n/a |

## 4. USB3300 PHY: RESET, VBUS, ID, USB connector

**Everything in this section is MEM or OPEN.** The firmware assumes nothing here
except what is stated; the choices are laid out in DECISIONS.md sections 3-4.

| Item | Firmware assumption | Assumed in | Status |
|---|---|---|---|
| PHY RESET pin | **None.** Firmware does not drive any reset line and does not wait for the PHY clock. It requires the PHY to be out of reset with its 60 MHz clock running when `board_init()` enables the OTG_HS clocks | `board.c` `board_init()` | **OPEN** (polarity and whether a pulse is required: unknown - datasheet not read) |
| PHY VBUS pin | Connected to connector VBUS however the datasheet requires (series R / capacitor: unknown). Firmware does **not** read VBUS: TinyUSB 0.19.0 forces "B-session valid" in the core (`dcd_dwc2.c`) | `third_party/tinyusb/.../dcd_dwc2.c`, no code of ours | **OPEN** - DECISIONS.md section 4. Verify the rule that a bus-powered device may connect without sensing VBUS is acceptable for your case |
| MCU VBUS sense | None (PB13 is ULPI D6). No GPIO reserved for it | - | OPEN |
| PHY ID pin | Not used: firmware forces device mode (`GUSBCFG.FDMOD`, TinyUSB). Tie per the datasheet (MEM: floating/high for a B-device) | `dcd_dwc2.c` | MEM |
| Connector D+ / D- | Go to the **PHY's DP/DM**, **not** to STM32 PA11/PA12 | - | check. PA11/PA12 (`OTG_FS`) are unused; OTG_FS is never enabled |
| Connector shield/ESD/common-mode choke | Not a firmware matter | - | design choice |
| PHY external charge-pump enable (CPEN), RBIAS resistor, regulator caps | Not used by firmware (device mode only) | - | MEM: RBIAS value and 1.8 V regulator cap come from the datasheet - check |
| PHY VDDIO vs MCU 3.3 V | Must match MCU I/O level | - | MEM: check datasheet |

Also verify: the ULPI nets are routed as a 60 MHz clock + 8-bit bus (length/impedance
guidance is in the PHY datasheet - not a firmware assumption).

## 5. STM32F405 boot, reset, debug

| Item | Firmware assumption | Assumed in | Status |
|---|---|---|---|
| BOOT0 | Low at reset so the MCU boots from flash at 0x08000000 (vector table there) | `dongle/STM32F405RGTx_FLASH.ld` | MEM (boot pin behaviour) - use a pull-down, optionally a jumper/button to enter the ROM bootloader |
| BOOT1 (PB2 on the F405) | Not used; leave per datasheet | - | MEM |
| NRST | Pull-up, optional button; also wired to the SWD header | - | MEM |
| ROM USB-DFU | The STM32 ROM bootloader's USB DFU uses the **OTG_FS pins (PA11/PA12)**, not the ULPI port (MEM, ST AN2606 not available). Dongle has D+/D- on the PHY, so **DFU over USB is not available** unless PA11/PA12 are brought to a pad/connector | - | **OPEN** - if you want DFU, provide PA11/PA12 access or plan on SWD only |
| SWDIO | PA13 | firmware never touches it (`gpio_init()` only configures PA3/4/5/6/7, PB0/1/5/10-13, PC0/2/3/10-12) | SRC-1 (pinctrl `sys_jtms_swdio_pa13`) |
| SWCLK | PA14 | same | SRC-1 (pinctrl `sys_jtck_swclk_pa14`) |
| SWO (optional) | PB3 (free in this plan) | not used | MEM |
| VTref / GND on header | Standard debug header practice | - | design choice |

## 6. Power (not firmware assumptions, listed so nothing is missed)

- MCU: VDD, VDDA, VCAP1/VCAP2 capacitors and VBAT per the F405 datasheet (MEM).
- 5 V VBUS -> 3.3 V regulator sized for MCU + PHY + nRF24 (currents: check datasheets; not looked up).
- Common ground between MCU, PHY and nRF24 module.

## 7. Keyboard board (nRF52840) - the firmware here is a bring-up app only

The real keyboard board and the ZMK integration are not in this repository, so
most of this cannot be checked against the code. What `keyboard_esb_addon/`
assumes:

| Item | Assumption | Assumed in | Status |
|---|---|---|---|
| 32 MHz HFXO crystal | Present: `CONFIG_ESB_CLOCK_INIT=y` starts HFCLK in `esb_init()` | `prj.conf`, `esb_tx.c` | SRC (Kconfig help text); board requirement MEM |
| Antenna / matching network | Board specific | - | design |
| LF clock source (32.768 kHz crystal or RC) | Zephyr board default; unknown for your board | - | OPEN |
| GPIO | The bring-up app uses **none** (no matrix, no LEDs). Matrix rows/cols are not in this repo | `src/main.c` | n/a |
| SWD | Board specific | - | design |
| Build board | Compiled for `nrf52840dk/nrf52840` only | `tools/build_keyboard.sh` | your board's devicetree/Kconfig not checked |
