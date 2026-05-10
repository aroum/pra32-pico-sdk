#pragma once

// ======================================================================
// System Settings
// ======================================================================
#define CPU_SPEED_KHZ 170000
#define VERSION_STRING "v3.3.2    "

// Pico compatibility layer
typedef bool boolean;
typedef uint8_t byte;

// ======================================================================
// Board Settings
// ======================================================================
#define BOARD_PRA32 1
#define BOARD_NIZKOTENO 2
#define BOARD_OMSK 3

// Set the current board here
#ifndef CURRENT_BOARD
#define CURRENT_BOARD BOARD_NIZKOTENO
#endif

// ======================================================================
// Nizkoteno Settings
// ======================================================================
#if CURRENT_BOARD == BOARD_NIZKOTENO
#define PRODUCT_NAME "Nizkoteno"

// 19, 20, 21, 24, 26, 27, 28, 29

// Button settings
#define BUTTON_DEBOUNCE_DELAY_MS 5
#define NUM_BUTTONS 10

static const uint8_t DIRECT_BUTTON_PINS[NUM_BUTTONS] = {9, 8, 6, 5, 10,
                                                        7, 4, 2, 1, 3};

// LED Settings
#define STATUS_LED 15

#define WS2812_PIN 11
#define WS2812_NUM 10
#define WS2812_ORDER_MAP {9, 8, 7, 6, 0, 1, 2, 3, 4, 5}

// MIDI Settings
#define MIDI_UART_INSTANCE uart1
#define MIDI_UART_TX_PIN 19
#define MIDI_UART_RX_PIN 20
#define MIDI_UART_SPEED 31250
#define MIDI_CHANNEL 0 // 0-indexed (Channel 1)

// Audio Settings
// Choose audio output type: 1 for I2S (DAC), 0 for PWM
#define USE_I2S_AUDIO 0
// Use PIO for audio (always true for I2S, optional for PWM)
#define USE_PIO_AUDIO 0

#if USE_I2S_AUDIO == 1
// I2S Settings (Pimoroni Audio Pack default)
#define I2S_DATA_PIN 9
#define I2S_BCLK_PIN 10
#define I2S_SWCLK_PIN 11
#define I2S_DAC_MUTE_OFF_PIN 22
#endif // USE_I2S_AUDIO

#if USE_I2S_AUDIO == 0
// PWM Settings
#define PWM_AUDIO_L_PIN 28
#define PWM_AUDIO_R_PIN 29

// --- Audio Quality ---
// 1: Error Diffusion (Highest quality, best for Nizkoteno)
// 0: Dithering (Standard quality)
#define USE_PWM_AUDIO_ERROR_DIFFUSION 0

// Internal PRA32-U driver mapping
#define PRA32_U_USE_PWM_AUDIO_INSTEAD_OF_I2S
#endif

// Multi-core synth processing (Required for 4-voice polyphony)
#define PRA32_U_USE_2_CORES_FOR_SIGNAL_PROCESSING

#endif // CURRENT_BOARD == BOARD_NIZKOTENO

// ======================================================================
// Omsk Settings
// ======================================================================
#if CURRENT_BOARD == BOARD_OMSK
#define PRODUCT_NAME "Omsk"

static const uint8_t MATRIX_COLS[4] = {11, 10, 7, 6};
static const uint8_t MATRIX_ROWS[4] = {23, 24, 13, 12};

// Debounce settings
#define BUTTON_DEBOUNCE_DELAY_MS 10
#define NUM_OMSK_BUTTONS 16

// LED
#define STATUS_LED -1 // No status LED for OMSK

#define WS2812_PIN 16
#define WS2812_NUM 22
#define WS2812_ORDER_MAP                                                       \
  {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21}

// Encoder Settings
static const uint8_t OMSK_ENCODER_PINS[4][2] = {
    {2, 3},   // Enc 1: A, B
    {4, 5},   // Enc 2: A, B
    {22, 17}, // Enc 3: A, B
    {18, 19}  // Enc 4: A, B
};

// Display Settings
// SSD1312 128x64 OLED display
#define CFG_ENABLE_OLED 1

#define OLED_DRIVER SSD1312_128x64
#define OLED_I2C_INSTANCE i2c1
#define OLED_SCL_PIN 15
#define OLED_SDA_PIN 14
#define OLED_I2C_ADDRESS 0x3C
#define OLED_BRIGHTNESS_PERCENT 50

// MIDI Settings
#define MIDI_UART_INSTANCE uart1
#define MIDI_UART_TX_PIN 0 // OUT
#define MIDI_UART_RX_PIN 1 // IN
#define MIDI_UART_SPEED 31250
#define MIDI_CHANNEL 0 // 0-indexed (Channel 1)

// Audio Settings
// Choose audio output type: 1 for I2S (DAC), 0 for PWM
#define USE_I2S_AUDIO 0
// Use PIO for audio (always true for I2S, optional for PWM)
#define USE_PIO_AUDIO 1

// I2S Settings (Pimoroni Audio Pack default)
#if USE_I2S_AUDIO == 1
#define I2S_DATA_PIN 9
#define I2S_BCLK_PIN 10
#define I2S_SWCLK_PIN 11
#define I2S_DAC_MUTE_OFF_PIN 22
#endif // USE_I2S_AUDIO

// PWM Settings
#if USE_I2S_AUDIO == 0
#define PWM_AUDIO_L_PIN 28
#define PWM_AUDIO_R_PIN 29
#endif // USE_PIO_AUDIO

#endif // CURRENT_BOARD == BOARD_OMSK

// ======================================================================
// PRA32 Settings
// ======================================================================
#if CURRENT_BOARD == BOARD_PRA32
#define PRODUCT_NAME "PRA32-U"

// MIDI Settings
#define MIDI_UART_INSTANCE uart1
#define MIDI_UART_TX_PIN 14
#define MIDI_UART_RX_PIN 15
#define MIDI_UART_SPEED 31250
#define MIDI_CHANNEL 0 // 0-indexed (Channel 1)

#define USE_CONTROL_PANEL 1

// Button/Key Input
#define USE_CONTROL_PANEL_KEY_INPUT 1
#define KEY_INPUT_ACTIVE_LEVEL HIGH
#define KEY_INPUT_PIN_MODE INPUT_PULLDOWN
#define KEY_PREV_PIN 9
#define KEY_NEXT_PIN 8
#define KEY_PLAY_PIN 6

// Analog Input (Pots)
#define USE_CONTROL_PANEL_ANALOG_INPUT 1
#define ANALOG_INPUT_REVERSED true
#define ANALOG_INPUT_CORRECTION (-504)

// OLED Display (I2C)
// SSD1312 128x32 OLED display
#define OLED_DRIVER SSD1306_128x64
#define USE_CONTROL_PANEL_OLED_DISPLAY 1
#define OLED_I2C_INSTANCE i2c1
#define OLED_SDA_PIN 5
#define OLED_SCL_PIN 7
#define OLED_I2C_ADDRESS 0x3C

// Audio Settings
// Choose audio output type: 1 for I2S (DAC), 0 for PWM
#define USE_I2S_AUDIO 0
// Use PIO for audio (always true for I2S, optional for PWM)
#define USE_PIO_AUDIO 1

// I2S Settings (Pimoroni Audio Pack default)
#if USE_I2S_AUDIO == 1
#define I2S_DATA_PIN 9
#define I2S_BCLK_PIN 10
#define I2S_SWCLK_PIN 11
#define I2S_DAC_MUTE_OFF_PIN 22
#endif // USE_I2S_AUDIO

// PWM Settings
#if USE_I2S_AUDIO == 0
#define PWM_AUDIO_L_PIN 28
#define PWM_AUDIO_R_PIN 29
#endif // USE_PIO_AUDIO

#endif // CURRENT_BOARD == BOARD_PRA32