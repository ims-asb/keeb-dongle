# SUMMARY.md

Nothing here has run on hardware. "Verified" means checked against an
open-source reference or by compiling/running on the VM, never on a board.
Current state at a glance: STATUS.md. Sources per value: VERIFY.md.

## Second pass (2026-09-30)

### Done
1. **docs/SCHEMATIC_CHECKLIST.md**: every pin/net/external connection the firmware assumes (ULPI, SPI3/nRF24, CSN/CE/IRQ, HSE, USB3300 RESET/VBUS/ID, USB connector, BOOT/NRST, SWD, keyboard-board items), each with direction, assuming file, and status (SRC-2 / SRC-1 / MEM / OPEN).
2. **DECISIONS.md sections 3-4**: USB3300 RESET (4 options) and VBUS (3 options), with what each needs from the schematic. Nothing chosen. Section 5: nRF24 IRQ. Section 6: guesses I baked in.
3. **Stuck keys fixed**: keyboard resends the current state every 20 ms while any key is held or after a TX failure (`shared/keepalive.h`, `n96_esb_tick()`); dongle releases all keys after 100 ms of silence with keys held (`dongle/src/core.c`). Both numbers are unmeasured guesses.
4. **HID boot protocol**: interface is boot-capable (subclass 1 / protocol 1); `tud_hid_set_protocol_cb` switches between the 14-byte NKRO report and an 8-byte 6KRO boot report (ErrorRollOver beyond six keys); current state is re-sent on every switch and on mount.
5. **IRQ**: evaluated, **not switched**. Reasoning and options in DECISIONS.md section 5 (no measured benefit; would make firmware depend on an unconfirmed net).
6. **STATUS.md** rewritten to the current state. No README touched.
7. **Datasheet re-check**: `docs/` still has no datasheets, so nothing could be re-verified; new UNVERIFIED rows added to VERIFY.md section 7.
8. Refactor to make this testable: radio payload -> state -> report logic moved from `main.c` into hardware-free `dongle/src/core.c`.

### Verified (VM only)
- Host tests, all passing: protocol 40, keepalive 26, report/boot report 79, dongle core 5058 (includes 5 s of simulated held-key time with resends), nRF24 30, USB descriptors 125 + 133 (normal/test builds), rate tool 9 Python tests. Deliberate mutations of the watchdog, resend pacing, boot-report ordering/rollover, retry flag and interface protocol byte were each caught.
- Dongle (normal and test mode) and keyboard app still compile: dongle 16.1 kB text; keyboard 30.9 kB flash.
- TinyUSB 0.19.0 boot-protocol API, default report protocol, SET_IDLE handling, forced B-session valid, `tud_connect/disconnect`: read in source (VERIFY.md section 7).

### Unverified
- Everything a datasheet decides (see VERIFY.md; USB3300 facts in the checklist are from memory).
- BIOS/UEFI actually typing with the boot report; OS acceptance of an NKRO descriptor on a boot-subclass interface; that the boot-report format/ErrorRollOver details match the HID spec (not available here).
- The 20 ms / 100 ms constants under real RF loss; false-release rate.
- Everything about real RF, USB enumeration, timing.

### Needs you (stopped rather than guess)
- **USB3300 datasheet**: RESET polarity/pulse, VBUS pin requirements, ID pin, reference clock. Then pick RESET (A-D) and VBUS (A-C) in DECISIONS.md.
- **Power topology**: bus-powered only? (VBUS option A assumes yes.)
- **HSE frequency**, real **keyboard board**, real **VID/PID**, and whether you want USB-DFU (would need PA11/PA12 exposed) or SWD only.

### Next
1. Run docs/SCHEMATIC_CHECKLIST.md against the KiCad schematic; report mismatches.
2. Drop datasheets into `docs/`; re-check the UNVERIFIED/MEM rows.
3. Hardware bring-up order: test-mode firmware -> `lsusb -t` shows 480M -> `tools/rate_test.py`; then radio with the keyboard test app; then BIOS boot-protocol check.
4. Decide DECISIONS.md sections 1-5; measure RF loss/latency, then tune the resend/timeout constants.
5. Key-to-usage table needs the KLE/KiCad files.

---

## First pass (2026-09-29) - kept for reference
### Done
1. **Reference check** of nRF24 registers, TinyUSB names, ESB names (VERIFY.md sections 1-3).
2. **board.c** (clocks, ULPI on AF10, SPI3, GPIO, SysTick, `OTG_HS_IRQHandler` -> `tusb_int_handler(1)`, `tusb_time_millis_api`). Your pin plan was kept unchanged.
3. **HS descriptors**: device qualifier, other-speed config, string descriptors (serial from the MCU unique ID), plus `tusb_config.h`.
4. **Builds**: dongle (CMake, arm-none-eabi-gcc 13.2.1, normal + `N96_TEST_MODE`) and keyboard ESB test app (NCS v3.4.1, `nrf52840dk/nrf52840`) both compile and link. Dongle: 15.8 kB text; keyboard: 30.6 kB flash.
5. **Host tests** (`make -C tests/host test`, gcc + ASan/UBSan): protocol (40 checks), report builder + HID descriptor walk (38), nRF24 driver vs mock SPI transcript (30), USB descriptor structure normal/test builds (119 + 129), rate-tool logic (9 Python tests). All pass.
6. **Test tools**: `N96_TEST_MODE` firmware (vendor-defined 16-byte report with a sequence counter, sent whenever the endpoint is ready, no radio), `tools/rate_test.py`, and `docs/TESTING.md` (USBTreeView / `lsusb -t` speed check, how to read results).
7. **DECISIONS.md** with options and tradeoffs for ESB+BLE radio sharing and RF-leg latency. Nothing chosen.

Not done: **task 7** - the repo contains no KLE JSON or KiCad files, so there is nothing to derive the key-to-usage table or matrix transform from. The `key_bitmask` = usage-ID contract in `shared/protocol.h` is unchanged.

### Verified (against a reference or by running on the VM)
- nRF24 register addresses, command bytes, CONFIG/FEATURE/DYNPD/STATUS bit values, RF_SETUP 0x0E derivation: match RF24's `nRF24L01.h`. **Secondary source; no datasheet was reachable.**
- ESB struct/field/function/enum names: match `sdk-nrf/include/esb.h` (v3.4.1) and compile.
- TinyUSB 0.19.0 macro/callback names and signatures: match source.
- ULPI pins D0-D7/CK/STP on AF10: match the CubeF4 HID_Standalone example and the F405RGTx pinctrl data; DIR=PC2, NXT=PC3 and SPI3 PC10-12 AF6: match the pinctrl data only.
- Report-descriptor size (14 B NKRO, 16 B test) by parsing the descriptor bytes; descriptor lengths chain; bit N -> usage N.
- The nRF24 driver emits the exact transcript in `tests/host/test_nrf24.c`; five deliberate mutations of the driver were each caught.

### Bugs found and fixed by checking
- `esb_tx.c`: local `esb_event_handler` collided with the `esb_event_handler` typedef in `esb.h` (compile error).
- `main.c`: no `OTG_HS_IRQHandler`, so USB would never have serviced interrupts; `tusb_init()` was called without a rhport (OTG_HS is rhport 1).
- HS device with no device-qualifier / other-speed callbacks: TinyUSB's weak defaults return NULL and the requests stall.
- nRF24 init: no power-on wait, PWR_UP written first, CE raised immediately, no read-back. Now: config while powered down, PWR_UP last, 5 ms, then CE, plus a CONFIG/RF_CH read-back (`nrf24_init_prx` returns false if the module doesn't answer).
- `n96_esb_send_keys` ignored `esb_set_*` return values in init; now checked.
- Stale "Gazell" comments in `protocol.h` removed.
- The files were uploaded flat, but the sources assume `dongle/`, `keyboard_esb_addon/`, `shared/`; moved accordingly (git history preserved).

### Unverified
- **Anything a datasheet settles**: nRF24 timing (100 ms power-on, 5 ms PWR_UP wait are RF24's conservative values), SPI mode 0, F405 clock tree (168 MHz PLL, APB dividers, 5 wait states), SPI BR encoding, RAM/CCM/flash sizes in the linker script.
- **On-air compatibility** of nRF52840 ESB (DPL + 16-bit CRC) with the nRF24L01+. Nordic's docs give `ESB_LEGACY_CONFIG` (fixed payload, 8-bit CRC) as the compatibility recipe; the DPL/CRC16 combination used here is not documented as tested.
- **USB High-Speed enumeration**, ULPI bring-up, VBUS handling with the USB3300 in ULPI mode, HID descriptor acceptance by real OSes, the rate tool against the real firmware.
- ESB calls made from the event handler (`esb_flush_tx`) on target.
- The keyboard build is for `nrf52840dk/nrf52840` with no Bluetooth and no ZMK; the real board and ZMK integration are untested.
- Known gaps then: stuck key on a lost release (fixed in the second pass), no boot protocol (fixed), polling instead of IRQ (evaluated, kept), no LED/output report (still true).
- "8 kHz" is the USB polling interval request only (bInterval=1 at HS). Nothing here claims or measures end-to-end 8 kHz.

### Next
1. Answer the hardware questions: dongle **HSE frequency**; **USB3300 RESET pin** and VBUS wiring (not in the pin plan; from memory and unverified: the USB3300 needs its reset line inactive and its 60 MHz clock running before the OTG_HS core starts); real **keyboard board** and how ZMK is built; real **VID/PID**.
2. Get the datasheets (nRF24L01+, STM32F405, USB3300) into `docs/` and re-run the UNVERIFIED rows in VERIFY.md.
3. Bring-up order on hardware: test-mode firmware -> check `lsusb -t` shows 480M -> `tools/rate_test.py`; then radio: keyboard test app -> dongle, watching `rf_missed`.
4. Decide the two items in DECISIONS.md; add the heartbeat/resend for stuck keys; consider the nRF24 IRQ pin for RX.
5. Provide the KLE JSON / KiCad files to build the key-to-usage table and matrix transform.

### Reproducing
    tools/fetch_deps.sh && make -C tests/host test && python3 -m unittest tests/host/test_rate_tool.py
    cmake -S dongle -B build/dongle -G Ninja -DN96_HSE_HZ=<hz> && cmake --build build/dongle
    NCS_DIR=<west workspace> tools/build_keyboard.sh     # recipe in the script header
