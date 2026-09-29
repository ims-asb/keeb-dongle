# Host-side tests: build with gcc, run on any Linux/macOS box. No hardware.
CC      ?= gcc
CFLAGS  += -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all
SRC      = ../../dongle/src
INC      = -I. -I../../shared -I$(SRC)
BUILD    = ../../build/host-tests

TESTS    = test_protocol test_report test_nrf24 test_usb_descriptors test_usb_descriptors_testmode
TUSB     = ../../third_party/tinyusb/src

.PHONY: test clean
test: $(addprefix $(BUILD)/,$(TESTS))
	@set -e; for t in $(TESTS); do $(BUILD)/$$t; done

$(BUILD)/test_protocol: test_protocol.c ../../shared/protocol.h check.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_protocol.c -o $@
$(BUILD)/test_report: test_report.c $(SRC)/report.c $(SRC)/report.h check.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_report.c $(SRC)/report.c -o $@
$(BUILD)/test_nrf24: test_nrf24.c $(SRC)/nrf24l01p.c $(SRC)/nrf24l01p.h check.h | $(BUILD)
	$(CC) $(CFLAGS) $(INC) test_nrf24.c $(SRC)/nrf24l01p.c -o $@

$(BUILD):
	mkdir -p $@
clean:
	rm -rf $(BUILD)

# Descriptor tests need TinyUSB headers: run tools/fetch_deps.sh first.
$(BUILD)/test_usb_descriptors: test_usb_descriptors.c $(SRC)/usb_descriptors.c check.h | $(BUILD)
	@test -d $(TUSB) || { echo "third_party/tinyusb missing - run tools/fetch_deps.sh"; exit 1; }
	$(CC) -std=gnu11 -Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all $(INC) -I$(TUSB) test_usb_descriptors.c $(SRC)/usb_descriptors.c -o $@
$(BUILD)/test_usb_descriptors_testmode: test_usb_descriptors.c $(SRC)/usb_descriptors.c check.h | $(BUILD)
	@test -d $(TUSB) || { echo "third_party/tinyusb missing - run tools/fetch_deps.sh"; exit 1; }
	$(CC) -std=gnu11 -DN96_TEST_MODE=1 -Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all $(INC) -I$(TUSB) test_usb_descriptors.c $(SRC)/usb_descriptors.c -o $@
