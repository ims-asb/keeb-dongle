# Nano96 8 kHz dongle - status

Untested skeleton. Nothing here has run on hardware. Roughly 25-30% of
the way to a first bring-up build (my estimate).

## Architecture (current)
keyboard nRF52840 --ESB 2.4 GHz--> dongle nRF24L01+ --SPI--> STM32F405
--ULPI--> USB3300 PHY --USB 2.0 High-Speed--> PC

## Corrections made in this revision
- **Gazell -> ESB.** The dongle radio is an nRF24L01+, which speaks
  Enhanced ShockBurst. Nordic documents the nRF5 ESB library as on-air
  compatible with nRF24L devices. The earlier Gazell code was the wrong
  protocol for this hardware and was removed.
- **SPI pin conflict.** SPI1's SCK pin (PA5) is ULPI_CK, and SPI3's
  PB3-5 option collides with ULPI_D7 (PB5). Plan: nRF24 on SPI3 using
  PC10/PC11/PC12, CSN=PA4, CE=PA6, IRQ=PA7. Use these in the schematic.
- Rewrote the dongle for STM32 (HAL + TinyUSB); the nRF5-SDK dongle
  file no longer applies.

## Real vs. still guessed
Verified against Nordic docs: ESB API names (esb_init,
esb_write_payload, esb_set_base_address_0, esb_set_prefixes,
esb_set_rf_channel), ESB_PROTOCOL_ESB_DPL, legacy-compat config exists.
From memory, NOT re-verified: nRF24L01+ register values (cross-check the
datasheet), TinyUSB macro/callback names, ULPI pin AF10 assignments
(D7=PB5 still worth a datasheet check), struct field names in esb.h.

## Open problems
1. ESB and ZMK's Bluetooth both want the nRF52840 radio - unresolved.
2. RF-leg latency is unmeasured; 8 kHz USB does not mean 8 kHz end-to-end.
3. board_init (clocks, ULPI AF10 pins, SPI3, GPIO) is not written.
4. HS device-qualifier / other-speed / string descriptors: TODO.
5. Physical-key -> HID-usage mapping table: not built.
6. USB VID/PID are placeholders.
