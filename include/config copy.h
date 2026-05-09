#pragma once

// ======================================================================
// Audio Settings
// ======================================================================
// Choose audio output type: 1 for I2S (DAC), 0 for PWM
#define USE_I2S_AUDIO 1

// I2S Settings (Pimoroni Audio Pack default)
#define I2S_DATA_PIN 9
#define I2S_BCLK_PIN 10
#define I2S_SWCLK_PIN 11
#define I2S_DAC_MUTE_OFF_PIN 22

// PWM Settings
#define PWM_AUDIO_L_PIN 28
#define PWM_AUDIO_R_PIN 27

// ======================================================================
// MIDI Settings
// ======================================================================
#define MIDI_UART_INSTANCE uart1
#define MIDI_UART_TX_PIN 4
#define MIDI_UART_RX_PIN 5
#define MIDI_UART_SPEED 31250
#define MIDI_CHANNEL 0 // 0-indexed (Channel 1)

// ======================================================================
// PRA32 Control Panel Settings
// ======================================================================
#define USE_CONTROL_PANEL 1

// Button/Key Input
#define USE_CONTROL_PANEL_KEY_INPUT 1
#define KEY_INPUT_ACTIVE_LEVEL HIGH
#define KEY_INPUT_PIN_MODE INPUT_PULLDOWN
#define KEY_PREV_PIN 16
#define KEY_NEXT_PIN 18
#define KEY_PLAY_PIN 20

// Analog Input (Pots)
#define USE_CONTROL_PANEL_ANALOG_INPUT 1
#define ANALOG_INPUT_REVERSED true
#define ANALOG_INPUT_CORRECTION (-504)

// OLED Display (I2C)
#define USE_CONTROL_PANEL_OLED_DISPLAY 1
#define OLED_I2C_INSTANCE i2c1
#define OLED_SDA_PIN 6
#define OLED_SCL_PIN 7
#define OLED_I2C_ADDRESS 0x3C

// ======================================================================
// System Settings
// ======================================================================
#define CPU_SPEED_KHZ 150000
#define VERSION_STRING "v3.3.2    "
