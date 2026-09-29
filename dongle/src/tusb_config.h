/* TinyUSB configuration (tag 0.19.0 option names - see VERIFY.md). */
#ifndef N96_TUSB_CONFIG_H
#define N96_TUSB_CONFIG_H

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU            OPT_MCU_STM32F4
#endif
#define CFG_TUSB_OS             OPT_OS_NONE
#define CFG_TUSB_DEBUG          0

/* STM32F4: OTG_FS = rhport 0, OTG_HS = rhport 1. The ULPI PHY is on OTG_HS. */
#define N96_TUD_RHPORT          1
#define CFG_TUD_ENABLED         1
#define CFG_TUD_MAX_SPEED       OPT_MODE_HIGH_SPEED

#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN      __attribute__((aligned(4)))

#define CFG_TUD_ENDPOINT0_SIZE  64

#define CFG_TUD_HID             1
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_MIDI            0
#define CFG_TUD_VENDOR          0

/* Must be >= the largest HID report we send (14 bytes NKRO, 16 bytes test). */
#define CFG_TUD_HID_EP_BUFSIZE  16

#endif
