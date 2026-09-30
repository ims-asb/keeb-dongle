# Nano96 dongle - status

Nothing here has run on hardware. Both firmwares compile; the hardware-free
logic has host tests. "8 kHz" means only the USB polling interval requested
(bInterval=1 at High Speed = 125 us), not end-to-end. Details: SUMMARY.md
(done/verified/unverified/next), VERIFY.md (every value and its source),
DECISIONS.md (open choices), docs/SCHEMATIC_CHECKLIST.md, docs/TESTING.md.

## Architecture
keyboard nRF52840 --ESB 2.4 GHz--> dongle nRF24L01+ --SPI3--> STM32F405
--ULPI--> USB3300 PHY --USB 2.0 High-Speed HID--> PC

## Layout
- `shared/` packet format (`protocol.h`), stuck-key logic (`keepalive.h`)
- `dongle/` STM32F405 firmware (CMake, TinyUSB 0.19.0 + HAL, pinned by `tools/fetch_deps.sh`)
- `keyboard_esb_addon/` nRF52840 ESB transmitter + bring-up app (NCS v3.4.1)
- `tests/host/` gcc tests, `tools/rate_test.py` USB rate tool

## Working (as far as the VM can tell)
- Dongle builds (normal and `N96_TEST_MODE`); keyboard app builds for `nrf52840dk/nrf52840` (no Bluetooth, no ZMK).
- `board.c` (clocks, ULPI AF10, SPI3, GPIO, OTG_HS IRQ), HS device qualifier / other-speed / string descriptors.
- NKRO report protocol plus HID boot protocol (SET_PROTOCOL): 14-byte NKRO vs 8-byte 6KRO boot report.
- Stuck-key protection: keyboard resends state every 20 ms while a key is held or after a TX failure; dongle releases all keys after 100 ms of silence with keys held. Numbers are unmeasured guesses.
- nRF24 driver with power-up sequencing and read-back, tested against a mock chip (exact SPI transcript).
- Test mode firmware (vendor report, sent whenever the endpoint is ready) and `tools/rate_test.py`.
- Host tests: protocol, keepalive, report/boot report, dongle core, nRF24, USB descriptors, rate tool.

## Verified vs. still unverified
Checked against open-source references only (RF24, TinyUSB, sdk-nrf `esb.h`,
CubeF4, Zephyr pinctrl data), never a datasheet: none was available.
Unverified: nRF24 timing and SPI mode, F405 clock tree and memory sizes,
USB3300 behaviour, ESB(DPL, CRC16)<->nRF24 on-air compatibility, HS
enumeration, boot-protocol behaviour in a real BIOS/UEFI, all RF timing.

## Open problems
1. ESB and ZMK Bluetooth both need the one nRF52840 radio - options in DECISIONS.md, none chosen.
2. RF-leg latency is unmeasured - measurement options in DECISIONS.md.
3. USB3300 RESET and VBUS handling - schematic decision, options in DECISIONS.md. Firmware assumes the PHY is out of reset with its clock running, bus-powered, no VBUS sensing.
4. nRF24 IRQ pin wired but unused (polling); evaluated, left as is (DECISIONS.md section 5).
5. Unknown board facts: HSE frequency (build define, default 8 MHz is an assumption), real keyboard board, USB VID/PID (placeholders 0xCAFE/0x0096, test firmware 0x0097).
6. Physical-key -> HID-usage table and matrix transform: not built (no KLE/KiCad files in the repo).
7. ROM USB-DFU probably unavailable on the dongle (D+/D- go to the PHY, not PA11/PA12) - SWD assumed for flashing.

## Known gaps
- No LED/output report (no caps-lock etc.); SET_IDLE accepted but idle rate not honoured.
- Boot report keys are in ascending usage order, not press order.
- Encoder/battery/heartbeat packet types are defined but not handled by the dongle.
- If the nRF24 doesn't answer at init, the dongle runs without radio (no retry).
