# Testing

Nothing in this repository has run on hardware. Everything below marked
"host" is verifiable without it; everything marked "needs hardware" is a
procedure for when the boards exist.

## Host tests (no hardware)

    tools/fetch_deps.sh            # once: clones pinned TinyUSB/HAL/CMSIS into third_party/
    make -C tests/host test        # C tests, gcc + ASan/UBSan
    python3 -m unittest tests/host/test_rate_tool.py

What they cover: packet pack/unpack and bounds, HID report building, the
NKRO/test report descriptors (parsed for total report size), the nRF24 driver's
exact SPI/CE/delay transcript against a mock chip model, and the structure of
the USB descriptors (lengths chain, qualifier and other-speed present,
bInterval byte). What they do not cover: RF behaviour, USB enumeration, timing.

## Firmware builds (compile only)

    # dongle (needs arm-none-eabi-gcc, cmake, ninja)
    tools/fetch_deps.sh
    cmake -S dongle -B build/dongle -G Ninja -DN96_HSE_HZ=<your crystal in Hz>
    cmake --build build/dongle
    cmake -S dongle -B build/dongle-test -G Ninja -DN96_HSE_HZ=<hz> -DN96_TEST_MODE=ON
    cmake --build build/dongle-test

    # keyboard
    NCS_DIR=<west workspace> tools/build_keyboard.sh

`N96_HSE_HZ` defaults to 8 MHz, which is an assumption, not a fact about the board.

## USB rate test (needs hardware)

1. Build and flash the `dongle-test` image (PID 0x0097, product string
   "Nano96 Dongle RATE TEST"). It sends a 16-byte vendor-defined report with a
   32-bit sequence counter whenever the endpoint is free; it does not use the
   radio and cannot type into the host.
2. **Check the enumeration speed first.** If it enumerated at Full Speed the
   rate test is meaningless for the HS claim.
   - Linux: `lsusb -t` - the device line should end in `480M` (12M = Full
     Speed). Also: `cat /sys/bus/usb/devices/*/speed` next to the matching
     `idVendor`/`idProduct` (`grep -l cafe /sys/bus/usb/devices/*/idVendor`), and
     `dmesg | tail` ("new high-speed USB device"). `lsusb -v -d cafe:0097`
     should show `bcdUSB 2.00`, a `Device Qualifier` section, and
     `bInterval 1` on the endpoint.
   - Windows: USBTreeView - select the device; "Device Bus Speed" should read
     "High Speed", and the endpoint descriptor `bInterval` should read 1.
3. Plug the dongle straight into a root-hub port. A Full-Speed hub in between
   caps the link at Full Speed.
4. `pip install hidapi` (the PyPI package named `hidapi`, module `hid`, not the
   unrelated package named `hid`), then `python3 tools/rate_test.py --duration 10`.
   On Linux, hidraw needs permissions, e.g. a udev rule:
   `SUBSYSTEM=="hidraw", ATTRS{idVendor}=="cafe", ATTRS{idProduct}=="0097", MODE="0666"`.

### Reading the result

| enumerated | reports/s (host userspace) | what it suggests |
|---|---|---|
| 480M | ~8000, no gaps | the endpoint is being polled every microframe and the host keeps up |
| 480M | well below 8000 | endpoint, host controller, OS or hidapi is the limit; not conclusive on its own |
| 480M | gaps in the sequence | reports were overwritten before the host read them |
| 12M | ~1000 | Full Speed: PHY/ULPI/clock/descriptors problem, not a rate problem |

The number is a USB-leg figure as seen by one host's software stack. It is not
end-to-end latency and says nothing about the RF link.

The rate tool and its counting logic are unit-tested with a fake device
(`tests/host/test_rate_tool.py`); it has never been run against the real firmware.
