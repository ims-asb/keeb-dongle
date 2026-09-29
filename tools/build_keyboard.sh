#!/bin/sh
# Build the keyboard-side ESB test app against an nRF Connect SDK workspace.
# This is the exact recipe that produced the compile check recorded in
# SUMMARY.md: NCS v3.4.1, Zephyr from that workspace, apt gcc-arm-none-eabi
# 13.2.1 (gnuarmemb), Python >= 3.12 with west + Zephyr's requirements-base.
#
#   NCS_DIR=/path/to/ncs-workspace ./tools/build_keyboard.sh [board]
#
# Workspace setup used (only the modules ESB needs):
#   west init -m https://github.com/nrfconnect/sdk-nrf --mr v3.4.1 $NCS_DIR
#   cd $NCS_DIR && west config manifest.project-filter -- \
#       "-.*,+zephyr,+hal_nordic,+nrfxlib,+cmsis_6,+cmsis,+picolibc,+nrfx"
#   west update -n -o=--depth=1
set -e
: "${NCS_DIR:?set NCS_DIR to the west workspace}"
BOARD="${1:-nrf52840dk/nrf52840}"   # the real keyboard board is not known
HERE="$(cd "$(dirname "$0")/.." && pwd)"
export ZEPHYR_BASE="$NCS_DIR/zephyr"
export ZEPHYR_TOOLCHAIN_VARIANT="${ZEPHYR_TOOLCHAIN_VARIANT:-gnuarmemb}"
export GNUARMEMB_TOOLCHAIN_PATH="${GNUARMEMB_TOOLCHAIN_PATH:-/usr}"
cd "$NCS_DIR"
west build -b "$BOARD" "$HERE/keyboard_esb_addon" -d "$HERE/build/keyboard" --no-sysbuild "$@"
