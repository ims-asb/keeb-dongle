#!/bin/sh
# Fetch the pinned third-party sources the dongle build needs into third_party/.
# Nothing is vendored into git. Tags were the newest at the time of writing.
set -e
cd "$(dirname "$0")/.."
mkdir -p third_party
clone() { # name url tag
    if [ ! -d "third_party/$1/.git" ]; then
        git clone -q --depth 1 --branch "$3" "$2" "third_party/$1"
    fi
    echo "$1 $(git -C third_party/$1 describe --tags 2>/dev/null) $(git -C third_party/$1 rev-parse --short HEAD)"
}
clone tinyusb            https://github.com/hathach/tinyusb.git                        0.19.0
clone stm32f4xx_hal_driver https://github.com/STMicroelectronics/stm32f4xx_hal_driver.git v1.8.5
clone cmsis_device_f4    https://github.com/STMicroelectronics/cmsis_device_f4.git     v2.6.9
clone cmsis_core         https://github.com/STMicroelectronics/cmsis_core.git           v5.9.0_20250520
