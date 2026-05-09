#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/flash.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "tusb.h"

#include "config.h"
#if CURRENT_BOARD == BOARD_NIZKOTENO || CURRENT_BOARD == BOARD_OMSK
#include "button_driver.h"
#include "led_driver.h"
#include "sequencer.h"
#endif

#if defined(CFG_ENABLE_OLED) || defined(USE_CONTROL_PANEL_OLED_DISPLAY)
#include "ui_oled.h"
#endif

#define HIGH 1
#define LOW 0
#define INPUT_PULLUP GPIO_IN
#define INPUT_PULLDOWN GPIO_IN
#define OUTPUT GPIO_OUT

void pinMode(uint pin, uint mode) {
    gpio_init(pin);
    gpio_set_dir(pin, mode == OUTPUT);
    if (mode == INPUT_PULLUP) gpio_pull_up(pin);
    if (mode == INPUT_PULLDOWN) gpio_pull_down(pin);
}

void digitalWrite(uint pin, bool val) {
    gpio_put(pin, val);
}

bool digitalRead(uint pin) {
    return gpio_get(pin);
}

#include "pra32-u-common.h"
#include "pico_eeprom.h"

// Define EEPROM instance
PicoEEPROM EEPROM;

// Forward declarations
#define PICO_SDK
#include "pra32-u-synth.h"

// Global instances
PRA32_U_Synth<> g_synth;
uint8_t g_midi_ch = MIDI_CHANNEL;

// Definitions from config.h adapted for synth
#define PRA32_U_VERSION                       VERSION_STRING
#define PRA32_U_MIDI_CH                       MIDI_CHANNEL
#define PRA32_U_USE_2_CORES_FOR_SIGNAL_PROCESSING

#if CURRENT_BOARD == BOARD_PRA32
#if USE_CONTROL_PANEL
#define PRA32_U_USE_CONTROL_PANEL
#if USE_CONTROL_PANEL_KEY_INPUT
#define PRA32_U_USE_CONTROL_PANEL_KEY_INPUT
#define PRA32_U_KEY_INPUT_ACTIVE_LEVEL          KEY_INPUT_ACTIVE_LEVEL
#define PRA32_U_KEY_INPUT_PIN_MODE              KEY_INPUT_PIN_MODE
#define PRA32_U_KEY_INPUT_PREV_KEY_PIN          KEY_PREV_PIN
#define PRA32_U_KEY_INPUT_NEXT_KEY_PIN          KEY_NEXT_PIN
#define PRA32_U_KEY_INPUT_PLAY_KEY_PIN          KEY_PLAY_PIN
#endif
#define PRA32_U_KEY_ANTI_CHATTERING_WAIT        (15)
#define PRA32_U_KEY_LONG_PRESS_WAIT             (375)

#if USE_CONTROL_PANEL_ANALOG_INPUT
#define PRA32_U_USE_CONTROL_PANEL_ANALOG_INPUT
#define PRA32_U_ANALOG_INPUT_REVERSED           ANALOG_INPUT_REVERSED
#define PRA32_U_ANALOG_INPUT_CORRECTION         ANALOG_INPUT_CORRECTION
#define PRA32_U_ANALOG_INPUT_THRESHOLD          (504)
#define PRA32_U_ANALOG_INPUT_DENOMINATOR        (504)
#endif

#if defined(USE_CONTROL_PANEL_OLED_DISPLAY)
#define PRA32_U_USE_CONTROL_PANEL_OLED_DISPLAY
#define PRA32_U_OLED_DISPLAY_I2C                OLED_I2C_INSTANCE
#define PRA32_U_OLED_DISPLAY_I2C_SDA_PIN        OLED_SDA_PIN
#define PRA32_U_OLED_DISPLAY_I2C_SCL_PIN        OLED_SCL_PIN
#define PRA32_U_OLED_DISPLAY_I2C_ADDRESS        OLED_I2C_ADDRESS
#define PRA32_U_OLED_DISPLAY_CONTRAST           (0xFF)
#define PRA32_U_OLED_DISPLAY_ROTATION           (true)
#endif
#endif
#endif

#define PRA32_U_UART_MIDI_SPEED               MIDI_UART_SPEED
#define PRA32_U_UART_MIDI_TX_PIN              MIDI_UART_TX_PIN
#define PRA32_U_UART_MIDI_RX_PIN              MIDI_UART_RX_PIN

#if CURRENT_BOARD == BOARD_PRA32
#include "pra32-u-control-panel.h"
#endif

#if USE_I2S_AUDIO
#include "i2s_audio.h"
#elif USE_PIO_AUDIO
#include "hardware/pio.h"
#include "audio_sdm.pio.h"
PIO audio_pio = pio0;
uint audio_sm_l, audio_sm_r;
static int32_t sdm_err_l = 0, sdm_err_r = 0;

static inline uint32_t sdm_o1_os32(int16_t sig, int32_t *err) {
    uint32_t out = 0;
    int32_t d = -32767 - sig;
    int32_t etmp;
    for (int j = 0; j < 32; j++) {
        etmp = d + *err;
        *err = etmp;
        if (etmp < 0) {
            *err += 65534;
            out |= (1u << j);
        }
    }
    return out;
}

void pwm_audio_init() {
    uint offset = pio_add_program(audio_pio, &audio_sdm_program);
    audio_sm_l = pio_claim_unused_sm(audio_pio, true);
    audio_sm_r = pio_claim_unused_sm(audio_pio, true);
    float bit_rate = (float)SAMPLING_RATE * 32.0f;
    audio_sdm_program_init(audio_pio, audio_sm_l, offset, PWM_AUDIO_L_PIN, bit_rate);
    audio_sdm_program_init(audio_pio, audio_sm_r, offset, PWM_AUDIO_R_PIN, bit_rate);
}

inline void pwm_audio_put_sample(int16_t left, int16_t right) {
    if (!pio_sm_is_tx_fifo_full(audio_pio, audio_sm_l)) {
        pio_sm_put(audio_pio, audio_sm_l, sdm_o1_os32(left, &sdm_err_l));
    }
    if (!pio_sm_is_tx_fifo_full(audio_pio, audio_sm_r)) {
        pio_sm_put(audio_pio, audio_sm_r, sdm_o1_os32(right, &sdm_err_r));
    }
}
#else
#define PWM_WRAP 1024
void pwm_audio_init() {
    gpio_set_function(PWM_AUDIO_L_PIN, GPIO_FUNC_PWM);
    gpio_set_function(PWM_AUDIO_R_PIN, GPIO_FUNC_PWM);
    uint slice_l = pwm_gpio_to_slice_num(PWM_AUDIO_L_PIN);
    uint slice_r = pwm_gpio_to_slice_num(PWM_AUDIO_R_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, PWM_WRAP - 1);
    pwm_init(slice_l, &cfg, true);
    if (slice_l != slice_r) pwm_init(slice_r, &cfg, true);
}

inline void pwm_audio_put_sample(int16_t left, int16_t right) {
    pwm_set_gpio_level(PWM_AUDIO_L_PIN, ((int32_t)left + 32768) * PWM_WRAP >> 16);
    pwm_set_gpio_level(PWM_AUDIO_R_PIN, ((int32_t)right + 32768) * PWM_WRAP >> 16);
}
#endif

// MIDI Callbacks
void handleNoteOn(byte channel, byte pitch, byte velocity) {
    if (channel == g_midi_ch) {
        g_synth.note_on(pitch, velocity);
    }
    
    // Send to UART MIDI
    if (uart_is_writable(MIDI_UART_INSTANCE)) {
        uart_putc(MIDI_UART_INSTANCE, 0x90 | (channel & 0x0F));
        uart_putc(MIDI_UART_INSTANCE, pitch & 0x7F);
        uart_putc(MIDI_UART_INSTANCE, velocity & 0x7F);
    }
    
    // Send to USB MIDI
    uint8_t packet[4] = { 0x09, (uint8_t)(0x90 | (channel & 0x0F)), (uint8_t)(pitch & 0x7F), (uint8_t)(velocity & 0x7F) };
    tud_midi_packet_write(packet);
}

void handleNoteOff(byte channel, byte pitch, byte velocity) {
    if (channel == g_midi_ch) {
        g_synth.note_off(pitch);
    }
    
    // Send to UART MIDI
    if (uart_is_writable(MIDI_UART_INSTANCE)) {
        uart_putc(MIDI_UART_INSTANCE, 0x80 | (channel & 0x0F));
        uart_putc(MIDI_UART_INSTANCE, pitch & 0x7F);
        uart_putc(MIDI_UART_INSTANCE, velocity & 0x7F);
    }
    
    // Send to USB MIDI
    uint8_t packet[4] = { 0x08, (uint8_t)(0x80 | (channel & 0x0F)), (uint8_t)(pitch & 0x7F), (uint8_t)(velocity & 0x7F) };
    tud_midi_packet_write(packet);
}

void handleControlChange(byte channel, byte number, byte value) {
    if (channel == g_midi_ch) {
        g_synth.control_change(number, value);
    }
}

void handleProgramChange(byte channel, byte number) {
    if (channel == g_midi_ch) {
        g_synth.program_change(number);
    }
}

void handlePitchBend(byte channel, int bend) {
    if (channel == g_midi_ch) {
        g_synth.pitch_bend((bend + 8192) & 0x7F, (bend + 8192) >> 7);
    }
}

void handleClock() {
#if CURRENT_BOARD == BOARD_PRA32
#if defined(PRA32_U_USE_CONTROL_PANEL)
    PRA32_U_ControlPanel_on_clock();
#endif
#endif
}

void handleStart() {
#if CURRENT_BOARD == BOARD_PRA32
#if defined(PRA32_U_USE_CONTROL_PANEL)
    PRA32_U_ControlPanel_on_start();
#endif
#endif
}

void handleStop() {
#if CURRENT_BOARD == BOARD_PRA32
#if defined(PRA32_U_USE_CONTROL_PANEL)
    PRA32_U_ControlPanel_on_stop();
#endif
#endif
}

// Control Panel Support Functions
uint8_t getCurrentControllerValue(byte channel, byte number) {
    if (channel == g_midi_ch) {
        return g_synth.current_controller_value(number);
    }
    return 0;
}

void getRandUint8Rrray(byte channel, uint8_t array[8]) {
    if (channel == g_midi_ch) {
        g_synth.get_rand_uint8_array(array);
    }
}

void writeParametersToProgram(byte channel, byte number) {
    if (channel == g_midi_ch) {
        g_synth.write_parameters_to_program(number);
    }
}

byte getTargetMIDICh(byte synth) {
    return (((g_midi_ch + synth) & 0x0F) + 1);
}

// Core 1: Control Panel and UI
void core1_main() {
    multicore_lockout_victim_init();
#if CURRENT_BOARD == BOARD_PRA32
    PRA32_U_ControlPanel_setup();
#endif
#if CURRENT_BOARD == BOARD_NIZKOTENO || CURRENT_BOARD == BOARD_OMSK
    button_driver_init();
    led_driver_init();
#endif
#if defined(CFG_ENABLE_OLED) || defined(USE_CONTROL_PANEL_OLED_DISPLAY)
    ui_oled_init();
#endif
    uint32_t loop_counter = 0;

    while (1) {
        boolean processed = g_synth.secondary_core_process();
        if (processed) {
            loop_counter++;
            if (loop_counter >= 16 * 400) {
                loop_counter = 0;
            }

#if CURRENT_BOARD == BOARD_PRA32
            PRA32_U_ControlPanel_update_analog_inputs(loop_counter);
            PRA32_U_ControlPanel_update_display_buffer(loop_counter);
            PRA32_U_ControlPanel_update_display(loop_counter);
#endif
        }
#if CURRENT_BOARD == BOARD_NIZKOTENO || CURRENT_BOARD == BOARD_OMSK
        button_driver_update();
        led_driver_update();
        sequencer_update(time_us_32());
#endif
        tight_loop_contents();
    }
}

// Core 0: Synth and MIDI
int main() {
    set_sys_clock_khz(CPU_SPEED_KHZ, true);
    
    stdio_init_all();
    tusb_init();

#if USE_I2S_AUDIO
    i2s_audio_init(SAMPLING_RATE);
#if defined(I2S_DAC_MUTE_OFF_PIN)
    pinMode(I2S_DAC_MUTE_OFF_PIN, OUTPUT);
    digitalWrite(I2S_DAC_MUTE_OFF_PIN, HIGH);
#endif
#else
    pwm_audio_init();
#endif

    // UART MIDI
    uart_init(MIDI_UART_INSTANCE, MIDI_UART_SPEED);
    gpio_set_function(MIDI_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(MIDI_UART_RX_PIN, GPIO_FUNC_UART);

    g_synth.initialize();
#if CURRENT_BOARD == BOARD_PRA32
    PRA32_U_ControlPanel_initialize_parameters();
#endif

    multicore_launch_core1(core1_main);

#if USE_I2S_AUDIO
    int32_t audio_buffer[64];
#endif

    while (1) {
        tud_task();

        // USB MIDI reading
        while (tud_midi_available()) {
            uint8_t packet[4];
            if (tud_midi_packet_read(packet)) {
                uint8_t cin = packet[0] & 0x0F;
                uint8_t status = packet[1];
                
                if (cin == 0x0F) { // Single-byte message
                    if (status == 0xF8) handleClock();
                    else if (status == 0xFA) handleStart();
                    else if (status == 0xFC) handleStop();
                } else {
                    uint8_t type = status & 0xF0;
                    uint8_t channel = status & 0x0F;
                    
                    if (type == 0x90) handleNoteOn(channel, packet[2], packet[3]);
                    else if (type == 0x80) handleNoteOff(channel, packet[2], packet[3]);
                    else if (type == 0xB0) handleControlChange(channel, packet[2], packet[3]);
                    else if (type == 0xC0) handleProgramChange(channel, packet[2]);
                    else if (type == 0xE0) handlePitchBend(channel, (packet[2] | (packet[3] << 7)) - 8192);
                }
            }
        }

        // UART MIDI reading
        while (uart_is_readable(MIDI_UART_INSTANCE)) {
            uint8_t b = uart_getc(MIDI_UART_INSTANCE);
            if (b == 0xF8) {
                handleClock();
            } else if (b == 0xFA) {
                handleStart();
            } else if (b == 0xFC) {
                handleStop();
            } else {
                static uint8_t state = 0;
                static uint8_t msg[3];
                static uint8_t msg_idx = 0;
                
                if (b >= 0x80) {
                    state = b;
                    msg_idx = 0;
                } else if (state != 0) {
                    msg[msg_idx++] = b;
                    uint8_t type = state & 0xF0;
                    uint8_t channel = state & 0x0F;
                    uint8_t needed = (type == 0xC0 || type == 0xD0) ? 1 : 2;
                    if (msg_idx >= needed) {
                        if (type == 0x90) handleNoteOn(channel, msg[0], msg[1]);
                        else if (type == 0x80) handleNoteOff(channel, msg[0], msg[1]);
                        else if (type == 0xB0) handleControlChange(channel, msg[0], msg[1]);
                        else if (type == 0xC0) handleProgramChange(channel, msg[0]);
                        else if (type == 0xE0) handlePitchBend(channel, (msg[0] | (msg[1] << 7)) - 8192);
                        msg_idx = 0;
                    }
                }
            }
        }

#if CURRENT_BOARD == BOARD_PRA32
        PRA32_U_ControlPanel_update_control();
#endif

#if USE_I2S_AUDIO
        // Audio generation for I2S (buffered)
        for (int i = 0; i < 64; i++) {
            int16_t left, right;
            left = g_synth.process(0, right);
            audio_buffer[i] = ((int32_t)left << 16) | (uint16_t)right;
        }
        i2s_audio_put_buffer(audio_buffer, 64);
#else
        // Audio generation for PWM (direct)
        static uint32_t next_sample_time = 0;
        uint32_t now = time_us_32();
        if (now >= next_sample_time) {
            next_sample_time = now + (1000000 / SAMPLING_RATE);
            int16_t left, right;
            left = g_synth.process(0, right);
            pwm_audio_put_sample(left, right);
        }
#endif
    }

    return 0;
}
