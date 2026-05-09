#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
 extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------
// #define CFG_TUSB_MCU                OPT_MCU_RP2040
#define CFG_TUSB_DEBUG              0

#define CFG_TUD_ENDPOINT0_SIZE      64

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------
#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE
#endif

#define CFG_TUD_MIDI                1
// #define CFG_TUD_CDC                 0
#define CFG_TUD_MSC                 0
#define CFG_TUD_HID                 0
#define CFG_TUD_BTH                 0

// MIDI FIFO size
#define CFG_TUD_MIDI_RX_BUFSIZE     (64)
#define CFG_TUD_MIDI_TX_BUFSIZE     (64)

// CDC FIFO size
#define CFG_TUD_CDC_RX_BUFSIZE      (256)
#define CFG_TUD_CDC_TX_BUFSIZE      (256)
#define CFG_TUD_CDC_EP_BUFSIZE      (64)

#ifdef __cplusplus
 }
#endif

#endif /* _TUSB_CONFIG_H_ */
