# DECISIONS.md - open problems (no option is chosen here)

Two problems are deliberately left undecided. Each section lists options and
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
