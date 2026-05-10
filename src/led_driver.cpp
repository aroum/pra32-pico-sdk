#include "led_driver.h"
#include "config.h"
#include "WS2812.hpp"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "button_driver.h"
#include "ui_state.h"
#include "sequencer.h"

#if CURRENT_BOARD == BOARD_NIZKOTENO || CURRENT_BOARD == BOARD_OMSK

static WS2812 *ledStrip = nullptr;
static uint32_t last_blink_time = 0;
static bool status_led_state = false;

void led_driver_init(void) {
    if (STATUS_LED != -1) {
        gpio_init(STATUS_LED);
        gpio_set_dir(STATUS_LED, GPIO_OUT);
    }
    uint sm = pio_claim_unused_sm(pio0, true);
    ledStrip = new WS2812(WS2812_PIN, WS2812_NUM, pio0, sm, WS2812::FORMAT_GRB);
}

void led_driver_update(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    sequencer_t *seq = sequencer_get();

    // Status LED blink to BPM
    uint32_t blink_interval = seq->step_duration_us / 1000;
    if (now - last_blink_time >= blink_interval) {
        status_led_state = !status_led_state;
        if (STATUS_LED != -1) gpio_put(STATUS_LED, status_led_state);
        last_blink_time = now;
    }

    static uint32_t last_led_update = 0;
    if (now - last_led_update >= 30) {
        last_led_update = now;
        ui_state_update_timers();
        if (ledStrip) {
            const uint8_t led_map[WS2812_NUM] = WS2812_ORDER_MAP;
            uint32_t button_mask = button_driver_get_logical_state();
            ui_state_t state = ui_state_get();
            uint8_t octave = ui_state_get_base_octave();
            chord_type_t chord_type = ui_state_get_chord_type();
            
            // Optimization: Only update if anything changed
            static uint32_t last_colors[WS2812_NUM] = {0};
            uint32_t current_colors[WS2812_NUM] = {0};
            bool changed = false;

#if CURRENT_BOARD == BOARD_OMSK
            // OMSK Logic (Maintained)
            for (int i = 0; i < 6; i++) current_colors[i] = 0;
            for (int i = 0; i < 16; i++) {
                bool pressed = (button_mask & (1 << i));
                uint8_t r = 0, g = 0, b = 0;
                if (state == UI_STATE_PIANO) {
                    if (i < 12) { r = 20; g = 20; b = 20; }
                    else if (i == 12) { r = 0; g = 0; b = 30; } else if (i == 13) { r = 30; g = 0; b = 0; }
                    else if (i == 14) { r = 30; g = 5; b = 15; } else if (i == 15) { r = 15; g = 0; b = 30; }
                    if (pressed) { r *= 4; g *= 4; b *= 4; }
                }
                current_colors[6 + i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
#else
            // NIZKOTENO Logic
            bool f1_pressed = (button_mask & (1 << BTN_PAD_9));
            bool f2_pressed = (button_mask & (1 << BTN_PAD_10));

            for (int i = 0; i < 10; i++) {
                bool pressed = (button_mask & (1 << i));
                uint8_t r = 0, g = 0, b = 0;

                if (state == UI_STATE_PIANO) {
                    if (f1_pressed && i < 8) {
                        if (chord_type == (chord_type_t)i) { r = 255; g = 255; b = 0; } else { r = 20; g = 20; b = 0; }
                    } else if (i < 7) {
                        r = pressed ? 255 : 20; g = r; b = r;
                    } else if (i == 7) {
                        if (octave == 60) { r = 0; g = 255; b = 0; }
                        else if (octave > 60) { float t = (float)(octave - 60) / 48; r = (uint8_t)(255 * t); g = (uint8_t)(255 * (1.0f - t)); }
                        else { float t = (float)(60 - octave) / 36; b = (uint8_t)(255 * t); g = (uint8_t)(255 * (1.0f - t)); }
                        if (!pressed) { r /= 8; g /= 8; b /= 8; }
                    }
                    if (i == 8) { 
                        // F1 / Chord LED
                        if (pressed) { r = 255; g = 0; b = 0; } 
                        else if (chord_type != CHORD_OFF) { r = 255; g = 0; b = 255; } // Pink if active
                        else { r = 30; g = 0; b = 0; } // Dim red if off
                    }
                    if (i == 9) { 
                        // F2 / Params button
                        if (pressed) { r = 0; g = 0; b = 255; } else { r = 0; g = 0; b = 30; } 
                    }
                } 
                else if (state == UI_STATE_SEQ) {
                    if (i < 8) {
                        uint8_t idx = seq->current_page * 8 + i;
                        seq_step_t *s = &seq->steps[idx];
                        if (s->is_stop_step) { r = 200; g = 0; b = 0; }
                        else if (s->num_notes > 0) {
                            if (s->muted) { r = 0; g = 40; b = 40; } // Dim Cyan for muted steps with notes
                            else { r = 0; g = 0; b = 200; }        // Blue for active steps with notes
                        } else {
                            r = 0; g = 0; b = 0; // Empty step: Off
                        }

                        if (seq->playing && seq->current_step == idx) { r = 255; g = 255; b = 255; }
                    }
                    if (i == 8) { if (pressed) { r = 255; g = 255; b = 255; } else { r = 20; g = 20; b = 20; } } // F1
                    if (i == 9) { // F2 Page color (Unified)
                        switch(seq->current_page) {
                            case 0: r = 0; g = 255; b = 255; break; // Cyan
                            case 1: r = 255; g = 255; b = 0; break; // Yellow
                            case 2: r = 0; g = 255; b = 0; break;   // Green
                            case 3: r = 255; g = 128; b = 0; break; // Orange
                        }
                        if (!pressed) { r /= 4; g /= 4; b /= 4; }
                    }
                }
                else if (state == UI_STATE_PARAMS) {
                    param_state_t p_state = ui_state_get_param_state();
                    uint8_t p_page = ui_state_get_param_page();
                    
                    if (p_state == PARAM_STATE_SELECT) {
                        // Colors per layout doc:
                        // osc(cyan) seq(yellow) filt(green) eg(orange) lfo(blue) amp(red) fx(magenta) misc(amber)
                        if (i == 0) { r = 0;   g = 255; b = 255; } // OSC: Cyan
                        else if (i == 1) { r = 255; g = 255; b = 0; }   // SEQ: Yellow
                        else if (i == 2) { r = 0;   g = 255; b = 0; }   // FILT: Green
                        else if (i == 3) { r = 255; g = 128; b = 0; }   // EG: Orange
                        else if (i == 4) { r = 0;   g = 0;   b = 255; } // LFO: Blue
                        else if (i == 5) { r = 255; g = 0;   b = 0; }   // AMP: Red
                        else if (i == 6) { r = 255; g = 0;   b = 255; } // FX: Magenta
                        else if (i == 7) { r = 255; g = 40; b = 0; }   // MISC: Amber

                        if (i < 8 && !pressed) { r /= 4; g /= 4; b /= 4; }

                        if (i == 8) { // To Piano: Pink
                            r = 255; g = 0; b = 128;
                            if (!pressed) { r /= 4; g = 0; b /= 4; }
                        }
                        if (i == 9) { // To Seq: Current Page color
                            switch(seq->current_page) {
                                case 0: r = 0;   g = 255; b = 255; break; // Cyan
                                case 1: r = 255; g = 255; b = 0;   break; // Yellow
                                case 2: r = 0;   g = 255; b = 0;   break; // Green
                                case 3: r = 255; g = 128; b = 0;   break; // Orange
                            }
                            if (!pressed) { r /= 4; g /= 4; b /= 4; }
                        }
                    } else {
                        // Inside a group
                        // Group color
                        uint8_t gr = 0, gg = 0, gb = 0;
                        switch(p_state) {
                            case PARAM_STATE_OSC:  gr = 0;   gg = 255; gb = 255; break; // Cyan
                            case PARAM_STATE_SEQ:  gr = 255; gg = 255; gb = 0;   break; // Yellow (SEQ)
                            case PARAM_STATE_FILT: gr = 0;   gg = 255; gb = 0;   break; // Green
                            case PARAM_STATE_EG:   gr = 255; gg = 128; gb = 0;   break; // Orange
                            case PARAM_STATE_LFO:  gr = 0;   gg = 0;   gb = 255; break; // Blue
                            case PARAM_STATE_AMP:  gr = 255; gg = 0;   gb = 0;   break; // Red
                            case PARAM_STATE_FX:   gr = 255; gg = 0;   gb = 255; break; // Magenta
                            case PARAM_STATE_MISC: gr = 255; gg = 40; gb = 0;   break; // Amber
                            default: break;
                        }
                        if (i < 8) {
                            // Only light pads that have a real parameter (not X)
                            bool has_param = ui_state_pad_has_param((uint8_t)i);
                            if (has_param) {
                                r = gr; g = gg; b = gb;
                                if (!pressed) { r /= 4; g /= 4; b /= 4; }
                            } else {
                                r = 0; g = 0; b = 0; // X = off
                            }
                        }
                        if (i == 8) { r = 255; g = 0; b = 0; if (!pressed) { r /= 4; } } // Back: Red
                        if (i == 9) { // Page toggle
                            int group = (int)p_state - (int)PARAM_STATE_OSC;
                            if (ui_state_group_has_page2(group)) {
                                if (p_page == 0) { r = 0; g = 255; b = 255; } else { r = 255; g = 255; b = 0; }
                                if (!pressed) { r /= 4; g /= 4; b /= 4; }
                                if (pressed) { r = 255; g = 255; b = 255; }
                            } else {
                                r = 0; g = 0; b = 0;
                            }
                        }
                    }
                }
                else if (state == UI_STATE_SEQ_EDIT) {
                    extern uint8_t editing_step_idx;
                    extern uint8_t editing_note_idx;
                    extern uint8_t seq_edit_octave;
                    seq_step_t *s = &seq->steps[editing_step_idx];
                    bool done_held = ui_state_seq_edit_is_done_held(button_mask);
                    
                    if (done_held && i < 8) {
                        if (s->chord == (chord_type_t)i) { r = 255; g = 255; b = 0; } else { r = 20; g = 20; b = 0; }
                    }
                    else if (i < 7) {
                        const uint8_t scale[7] = {0, 2, 4, 5, 7, 9, 11};
                        uint8_t current_slot_note = (s->num_notes > editing_note_idx) ? s->notes[editing_note_idx] : 0;
                        
                        bool is_current_note = (current_slot_note == seq_edit_octave + scale[i]);
                        bool is_sharp_part = false;
                        
                        // Check if this pad is part of a sharp (e.g., C or D when C# is selected)
                        if (current_slot_note != 0) {
                            if (i > 0 && current_slot_note == seq_edit_octave + scale[i-1] + 1 && scale[i] == scale[i-1] + 2) is_sharp_part = true;
                            if (i < 6 && current_slot_note == seq_edit_octave + scale[i] + 1 && scale[i+1] == scale[i] + 2) is_sharp_part = true;
                        }
                        
                        if (pressed || is_current_note || is_sharp_part) {
                            // Selected note is BLUE
                            r = 0; g = 0; b = 255;
                            if (s->muted) { r /= 4; g /= 4; b /= 4; }
                            if (pressed) { r = 255; g = 255; b = 255; } // White flash on press
                        } else {
                            // Unselected notes are dim white
                            r = 30; g = 30; b = 30;
                        }
                    }
                    else if (i == 7) { 
                        // Octave color (same as Piano)
                        if (seq_edit_octave == 60) { r = 0; g = 255; b = 0; }
                        else if (seq_edit_octave > 60) { float t = (float)(seq_edit_octave - 60) / 48; r = (uint8_t)(255 * t); g = (uint8_t)(255 * (1.0f - t)); }
                        else { float t = (float)(60 - seq_edit_octave) / 36; b = (uint8_t)(255 * t); g = (uint8_t)(255 * (1.0f - t)); }
                        if (!pressed) { r /= 4; g /= 4; b /= 4; }
                    }
                    if (i == 8) { r = 255; g = 0; b = 0; } // Done button: Red
                    if (i == 9) { 
                        // Next button / Note indicator
                        if (s->chord != CHORD_OFF) {
                            r = 255; g = 0; b = 0; // RED for Chord
                        } else {
                            switch(editing_note_idx) {
                                case 0: r = 0; g = 255; b = 255; break; // Cyan
                                case 1: r = 255; g = 255; b = 0; break; // Yellow
                                case 2: r = 0; g = 255; b = 0; break;   // Green
                                case 3: r = 255; g = 128; b = 0; break; // Orange
                            }
                        }
                        if (pressed) { r = 255; g = 255; b = 255; }
                    }
                }
                current_colors[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            }
#endif
            // Flash Blink Overlay
            uint8_t blink_pad = ui_state_get_flash_blink_pad();
            if (blink_pad != 255 && blink_pad < WS2812_NUM) {
                if (ui_state_get_flash_blink_state()) {
                    bool is_error = ui_state_get_flash_blink_is_error();
                    uint8_t r = is_error ? 255 : 0;
                    uint8_t g = is_error ? 0 : 255;
                    uint8_t b = 0;
                    current_colors[blink_pad] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                } else {
                    current_colors[blink_pad] = 0;
                }
            }

            // Parameter Bar (Overlay)
            uint32_t p_timer = ui_state_get_param_timer();
            if (p_timer > 0) {
                uint8_t p_val = ui_state_get_last_param_value();
                float filled = (p_val / 127.0f) * 4.0f;
                for (int i = 0; i < 4; i++) {
                    float seg = 0.0f;
                    if (filled >= (float)(i + 1)) seg = 1.0f; else if (filled > (float)i) seg = filled - (float)i;
                    if (seg > 0.01f) {
                        uint8_t br = (uint8_t)(seg * 255.0f);
                        if (p_timer < 20) br = (uint8_t)(br * (float)p_timer / 20.0f);
                        int led_idx = (CURRENT_BOARD == BOARD_OMSK ? 6 : 0) + i;
                        current_colors[led_idx] = ((uint32_t)br << 16) | ((uint32_t)br << 8) | br;
                    }
                }
            }

            // Compare and show
            for (int i = 0; i < WS2812_NUM; i++) {
                if (current_colors[i] != last_colors[i]) {
                    changed = true;
                    break;
                }
            }

            if (changed) {
                for (int i = 0; i < WS2812_NUM; i++) {
                    uint8_t r = (current_colors[i] >> 16) & 0xFF;
                    uint8_t g = (current_colors[i] >> 8) & 0xFF;
                    uint8_t b = (current_colors[i] >> 0) & 0xFF;
                    ledStrip->setPixelColor(led_map[i], r, g, b);
                    last_colors[i] = current_colors[i];
                }
                ledStrip->show();
            }
        }
    }
}
#else
void led_driver_init(void) {}
void led_driver_update(void) {}
#endif
