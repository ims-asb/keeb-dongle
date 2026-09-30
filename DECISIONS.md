# DECISIONS.md - open decisions (no option is chosen unless stated)

Sections 1-5 are deliberately left undecided. Each lists options and
their tradeoffs so the choice can be made with the costs visible. Facts are
tagged: **[src]** read in a checked-out reference, **[mem]** from memory /
unverified, **[unknown]** not established.

---

## 1. ESB and ZMK's Bluetooth both need the nRF52840's single radio

### What is established
- The nRF52840 has one RADIO peripheral. Two stacks cannot drive it at the
  same instant, so any coexistence is time-slicing. **[mem]**
- NCS's ESB library `select`s `MPSL` unconditionally (subsys/esb/Kconfig, v3.4.1);
  it is built on Nordic's multiprotocol service layer. **[src]**
- NCS has `CONFIG_ESB_MPSL_TIMESLOT`, marked **EXPERIMENTAL**. Its help text says
  the Timeslot API "can increase number of retransmissions, reduce ESB packets
  delivery rate, and cause the delay of retransmissions to vary."
  (subsys/esb/Kconfig). **[src]**
- Nordic ships `samples/esb/esb_ptx_ble`: ESB PTX running alongside a BLE
  peripheral via MPSL timeslots. Its BLE connection interval is 800 units
  (1 s at 1.25 ms/unit) - a relaxed BLE link, not a low-latency one
  (samples/esb/esb_ptx_ble/prj.conf). **[src]**
- The keyboard side in this repo (`keyboard_esb_addon/`) compiles as a
  standalone Zephyr app without Bluetooth. It has never been combined with ZMK. **[src: build]**
- Which BLE controller ZMK is built with in the user's tree (Zephyr's own
  link layer vs. Nordic's SoftDevice Controller), and whether MPSL can
  coexist with the former: **[unknown]** - not checked. ESB's MPSL dependency
  suggests the Nordic controller path, but that is inference, not verified.

### Options

| # | Option | Gains | Costs / risks |
|---|---|---|---|
| A | **ESB only** on the keyboard; drop Bluetooth from that firmware | Simplest radio use; ESB gets the radio whenever it wants; lowest RF-leg jitter potential | No Bluetooth for phones/tablets/multi-host; ZMK's BLE profiles unusable |
| B | **Time-slice both** with MPSL (`ESB_MPSL_TIMESLOT` + Nordic BLE controller) | Both protocols live at once; no user mode switch | Experimental per Nordic; ESB delivery rate and retransmit timing degrade and vary (**this hits the very latency this project cares about**); needs a ZMK build on the Nordic controller **[unknown feasibility]**; BLE connection parameters would have to leave slots for ESB |
| C | **Mutually exclusive modes** (BLE mode vs 2.4 GHz-dongle mode), switched by key/switch, stack shut down and the other brought up | Each mode gets the full radio; behaviour in each mode is simple to reason about | Switch delay and state hand-off; two radio stacks resident in flash; ZMK profile logic needs a "dongle" pseudo-profile; ESB init/teardown while BLE stack owns MPSL is **[unknown]** |
| D | **Different dongle radio**: dongle uses an nRF52 (ESB or BLE) instead of nRF24L01+ | Dongle can speak both worlds; could run BLE and ESB with the same coexistence problem on the *dongle* side instead | Hardware change; STM32+ULPI USB path still needed or replaced; BLE connection interval limits (7.5 ms minimum in classic BLE; newer spec revisions allow shorter **[mem, unverified]**) work against the low-latency goal |
| E | **Second radio SoC** on the keyboard for the 2.4 GHz link | No sharing at all | Board area, power, cost, a second firmware |

### Things that would inform the choice (not yet known)
- Which BLE controller the ZMK tree uses today.
- Whether the Bluetooth use case is required at all, or wired USB + dongle suffices.
- Measured ESB delivery under timeslot arbitration (see problem 2).

---

## 2. RF-leg latency is unmeasured

"8 kHz" in this repo means only the USB polling interval (bInterval=1 at High
Speed = 125 us). Nothing here measures or bounds key-press-to-radio-RX or
radio-RX-to-USB-report time.

### Latency contributors that exist in the current code (all unmeasured)
- ESB PTX: TX start after `esb_write_payload`, on-air time, ACK wait, and
  retransmissions (`retransmit_delay` 600 us start-to-start, `retransmit_count` 3
  in `ESB_DEFAULT_CONFIG` / esb_tx.c - the retransmit delay semantics are in
  NCS docs **[src]**). A lost packet costs at least one retransmit delay.
- Dongle: `poll_radio()` polls the nRF24 FIFO from the main loop; latency is
  the loop period, which depends on `tud_task()` time. The nRF24 IRQ line
  (PA7) is wired to a GPIO input but unused. **[src: main.c/board.c]**
- Dongle: one report is sent when the IN endpoint is free; with bInterval=1 at
  HS the host polls every 125 us if the link is High Speed. **[mem: USB spec]**
- Keyboard: matrix scan period/debounce, which is not in this repo at all.
- A dropped final key-release stays "held" until the next packet: there is no
  heartbeat/resend (esb_tx.c flushes on TX_FAILED). This is a correctness
  problem that also shapes worst-case latency. It is left as is.

### Ways to measure (none chosen)

| # | Method | Measures | Costs / limits |
|---|---|---|---|
| A | **GPIO timestamps on a logic analyzer**: keyboard raises a pin when it queues a packet (or on the key event); dongle raises a pin on nRF24 RX (or IRQ) and another when `tud_hid_report` is accepted | True one-way times for each leg, with a shared time base | Needs both boards, probe access, firmware instrumentation on both sides; USB-leg end is still the endpoint, not the host |
| B | **ACK-payload round trip**: dongle puts a timestamp in the ESB ACK payload; PTX computes RTT | RF round trip without a shared clock; halves to a rough one-way figure | Needs `EN_ACK_PAY` in the nRF24 driver and ESB RX-on-ACK handling (neither implemented); RTT includes retransmissions and turnaround; symmetry assumed |
| C | **Sequence + loss statistics only** (count `n96_seq_missed` under load, with/without Wi-Fi/BLE traffic) | Reliability, not latency | Cheap and already half-there (`rf_missed` counter in main.c); says nothing about delay |
| D | **Full-path external measurement**: press-to-host-event with a hardware tester or high-speed camera | The number a user actually feels | Mixes keyboard scan, RF, USB, host stack; hard to attribute |
| E | **Analytical bound** from packet format and ESB timing (airtime + turnaround + retransmit delay x N) | A best/worst-case envelope before hardware | Built from spec knowledge that is **[mem]** here (datasheets unreachable); an estimate, not a measurement |

### Interaction with problem 1
Options B (MPSL time-slicing) and C (mode switching) change RF-leg latency
and its variance; the measurement method should be chosen with the coexistence
option in mind, and it should be re-run after that option is chosen.

---

## 3. USB3300 RESET pin

### What is established
- The firmware drives **no** reset line and does not wait for the PHY clock; it
  needs the PHY out of reset with its 60 MHz clock present when `board_init()`
  enables the OTG_HS clocks (`dongle/src/board.c`). **[src: our code]**
- The reset pin's polarity, minimum pulse, power-up sequencing and whether it
  may simply be tied off: **[unknown]** - the USB3300 datasheet was not
  available. Any statement below about polarity or timing is from memory and
  must be checked before use.
- No spare MCU pin is assigned to it in the current pin plan (VERIFY.md section 4).

### Options

| # | Option | Needs from the schematic | Firmware change | Gains | Costs / risks |
|---|---|---|---|---|---|
| A | **Tie RESET to its inactive level** (resistor to GND or VDD, whichever the datasheet says is inactive) | RESET net polarity; one resistor | none | No pin, no code; simplest | PHY can only be reset by power-cycling the board; relies on the PHY starting cleanly from power-up alone (**unknown** whether it does) |
| B | **RC / supervisor power-on reset** on the pin | Polarity; an RC network or reset IC with the right output polarity and timing | none | No pin, no code; defined power-up pulse | Extra parts; timing values come from the datasheet (unknown here); no software recovery |
| C | **MCU GPIO drives RESET** | One free GPIO (candidates must be chosen from pins the plan does not use; none reserved today); a pull to the inactive level so the line is defined while the MCU is in reset/boot | Small: configure pin, assert, wait, release, wait for PHY clock before enabling OTG_HS; TinyUSB re-init path for a later PHY reset | Deterministic start-up order; firmware can reset a wedged PHY | Uses a pin; pin state during MCU reset and ROM-bootloader must be safe; no way to *observe* the PHY clock from the MCU other than by delay (PA5 is the ULPI clock input), so a fixed delay is a guess until measured |
| D | **Share the MCU's NRST** | Polarity of PHY reset vs NRST (active-low): if the PHY's reset is active-high, an inverter is needed | none | MCU reset also resets PHY; no dedicated pin | Inverter (maybe); PHY and MCU start together, so the "PHY clock present before core enable" ordering is not guaranteed by hardware |

### To decide
Read the datasheet's reset section (polarity, pulse width, whether an
unconnected/tied pin is allowed), then pick. Options A/B need no firmware change.

---

## 4. USB3300 VBUS handling

### What is established
- TinyUSB 0.19.0's dwc2 driver **forces B-session valid** in the OTG core
  (`GOTGCTL.BVALOEN | BVALOVAL`, `dcd_dwc2.c` `dcd_init()`), so the MCU does not
  need VBUS status from the PHY to connect, and the driver sets `XCVRDLY` for
  ULPI PHYs. `GUSBCFG.ULPIEVBUSD/ULPIEVBUSI` are cleared (internal VBUS
  indicator/drive). **[src: read in tag 0.19.0]**
- PB13 (`OTG_HS_VBUS` in FS-PHY mode) is ULPI D6 in this design; PB12
  (`OTG_HS_ID`) is D5. There is no MCU VBUS-sense pin on the HS core. **[src: pinctrl data + CubeF4 example]**
- `tud_connect()` / `tud_disconnect()` exist in the TinyUSB API (usbd.h) and could
  gate the pull-up on an application signal. **[src]**
- What the USB3300's VBUS pin needs (series resistor, capacitor, thresholds, what
  it reports over ULPI when VBUS is absent): **[unknown]**.
- The dongle is assumed **bus-powered**: VBUS is present whenever the board is
  powered from the USB port. **[assumption - confirm]**

### Options

| # | Option | Needs from the schematic | Firmware change | Gains | Costs / risks |
|---|---|---|---|---|---|
| A | **VBUS straight to the PHY VBUS pin; MCU does not sense it** (matches the current TinyUSB behaviour) | Connector VBUS -> PHY VBUS pin with whatever the datasheet requires; 5 V -> 3.3 V regulator | none | Simplest; what the firmware already assumes | The device connects whether or not a host is really there (fine when bus-powered; on a bench with the board powered from a debugger and a USB cable to another supply, it would drive the pull-up with VBUS absent) |
| B | **Divider from VBUS to a spare MCU GPIO**, gate connect/disconnect on it | A free GPIO; a divider (5 V -> <=3.3 V, or a 5 V-tolerant pin - pin tolerance **unverified**); pin not on PB13/PB12 | Small: read pin, `tud_connect()` / `tud_disconnect()`; debounce | Correct behaviour for a self-powered or dual-supply board; detects unplug | Pin + parts; boot-time ordering (VBUS already present at boot); not needed if bus-powered |
| C | **Stop forcing B-session valid and use the PHY's ULPI VBUS reports** | PHY VBUS wired as in A | Patch/fork the vendored TinyUSB dwc2 driver (it forces the override for all MCUs); verify the F4 core honours ULPI-reported session state | No extra pin; hardware-reported state | Modifies a third-party library; unverified that it works on this core/PHY; more to test on hardware |

### To decide
Confirm the power topology (bus-powered only?), read the PHY datasheet's VBUS
requirements, then pick. If bus-powered only, A needs no firmware change.

---

## 5. nRF24 IRQ pin: polling vs interrupt (evaluated, left as polling)

The IRQ line is wired to PA7 (input, pull-up) but the firmware polls the FIFO
from the main loop (`poll_radio()` -> `nrf24_read_payload()`).

**Why it was not switched**: the change is easy to code and to test with the
mock SPI, but it is not clearly worth doing now:
- Latency: polling reads `FIFO_STATUS` on every main-loop pass, which reacts at
  least as fast as a level check on IRQ. Gating the poll on IRQ would remove
  SPI traffic when idle (power/bus-time), not latency. No measurement shows the
  SPI polling costs anything yet (problem 2 is unmeasured).
- Risk: the firmware would then *depend* on the IRQ net being wired correctly
  and active-low (MEM: the module's IRQ is active-low), which the schematic has
  not confirmed. A fallback timed poll avoids a dead radio but silently caps
  worst-case latency at the fallback period if the net is wrong.
- The nRF24 CONFIG register currently leaves TX_DS and MAX_RT unmasked (0x0F),
  so IRQ would also assert on those flags; a PRX with auto-ack and no
  ACK payload should not raise them, but masking them (CONFIG 0x3F) would be
  part of the change and is unverified.

### Options
| # | Option | Gains | Costs |
|---|---|---|---|
| A | Keep polling (current) | Independent of IRQ wiring; lowest latency by construction | SPI read every loop pass |
| B | Level-gate the poll on PA7, plus a fallback poll every N ms | Idle SPI traffic drops; trivially testable with a mock | Depends on IRQ wiring; fallback period becomes worst-case latency if IRQ is unwired; mask TX_DS/MAX_RT (unverified) |
| C | EXTI falling-edge on PA7 sets a flag, main loop drains | Same benefit as B, no pin read in the loop | ISR + edge-loss handling (drain to empty, then re-check the pin) - more moving parts to verify without hardware |

Revisit after the schematic confirms IRQ, and after problem 2 shows whether
SPI polling matters.

---

## 6. Choices made in the second pass that you may want to revisit

These were implemented (not left open) but rest on guesses:
- **Stuck-key timing**: keyboard resends every 20 ms while a key is held (or after
  a failed transmission); dongle releases all keys after 100 ms of silence with
  keys held (`shared/keepalive.h`). Neither number is measured; 100 ms allows
  about five consecutive lost resends before a false release, and a held key costs
  about 50 packets/s of airtime and battery. The latency/reliability tradeoff
  belongs with problem 2.
- **Boot protocol**: ascending-usage order for the six boot keycodes (press order
  is not kept); ErrorRollOver (0x01) in all six slots beyond six keys, as the boot
  protocol requires (MEM: HID usage tables). SET_IDLE is accepted but the idle
  rate is not honoured (TinyUSB stores it; we never re-send on idle).
