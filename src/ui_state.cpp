#include "ui_state.h"
#include <stdio.h>
#include "pico/stdlib.h"
#include "pra32-u-constants.h"
#include "config.h"
#include "sequencer.h"

#if CURRENT_BOARD == BOARD_OMSK
#include "encoder_driver.h"
#endif

// Externs from main.cpp
extern void handleControlChange(uint8_t channel, uint8_t number, uint8_t value);
extern uint8_t getCurrentControllerValue(uint8_t channel, uint8_t number);
extern uint8_t g_midi_ch;
extern void handleProgramChange(uint8_t channel, uint8_t number);
static uint8_t current_preset = 0;

extern void handleNoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity);
extern void handleNoteOff(uint8_t channel, uint8_t pitch, uint8_t velocity);

static ui_state_t current_state = UI_STATE_PIANO;
static uint32_t previous_button_states = 0;
static uint8_t base_octave_note = 60; 
static chord_type_t current_chord_type = CHORD_OFF;
static param_state_t current_param_state = PARAM_STATE_SELECT;
static uint8_t current_param_page = 0;
static uint8_t last_param_value = 0;
static uint32_t param_display_timer = 0;

// Sequencer edit state
uint8_t editing_step_idx = 0;
uint8_t editing_note_idx = 0; // Current note slot (0-3)
uint8_t seq_edit_octave = 60;

static const uint8_t stepped_thresholds_2[] = {63, 127};
static const uint8_t stepped_thresholds_3[] = {31, 95, 127};
static const uint8_t stepped_thresholds_6[] = {12, 38, 63, 88, 114, 127};
static const uint8_t stepped_thresholds_amt_cycle[] = {4, 64, 124};

static void adjust_parameter(uint8_t cc, int8_t delta, const uint8_t* thresholds = nullptr, uint8_t num_thresholds = 0) {
    uint8_t current = getCurrentControllerValue(g_midi_ch, cc);
    uint8_t next = current;
    if (cc == 254) { // Virtual CC for BPM
        int val = (int)sequencer_get()->bpm + delta;
        if (val > 127) val = 127; if (val < 0) val = 0;
        sequencer_set_bpm((uint8_t)val);
        last_param_value = (uint8_t)val;
        param_display_timer = 66;
        return;
    }

    if (thresholds && num_thresholds > 0) {
        if (delta > 0) {
            bool found = false;
            for (uint8_t i = 0; i < num_thresholds; i++) { if (thresholds[i] > current) { next = thresholds[i]; found = true; break; } }
            if (!found) next = thresholds[0];
        } else if (delta < 0) {
            int found_idx = -1;
            for (int i = num_thresholds - 1; i >= 0; i--) { if (thresholds[i] < current) { next = thresholds[i]; found_idx = i; break; } }
            if (found_idx == -1) next = thresholds[num_thresholds - 1];
        } else {
            int current_idx = -1;
            for (int i = 0; i < num_thresholds; i++) { if (current <= thresholds[i]) { current_idx = i; break; } }
            next = thresholds[(current_idx + 1) % num_thresholds];
        }
    } else {
        int val = (int)current + delta;
        if (val > 127) val = 127; if (val < 0) val = 0;
        next = (uint8_t)val;
    }
    if (next != current || delta == 0) {
        handleControlChange(g_midi_ch, cc, next);
        last_param_value = next;
        param_display_timer = 66; 
    }
}

static inline bool is_pad_pressed(uint32_t states, logical_button_t pad) { return (states & (1 << pad)) != 0; }
static inline bool is_pad_just_pressed(uint32_t pressed, logical_button_t pad) { return (pressed & (1 << pad)) != 0; }
static inline bool is_pad_just_released(uint32_t changed, uint32_t states, logical_button_t pad) { return (changed & (1 << pad)) != 0 && !is_pad_pressed(states, pad); }

void ui_state_init(void) {
    current_state = UI_STATE_PIANO; previous_button_states = 0; base_octave_note = 60;
    current_chord_type = CHORD_OFF; current_param_state = PARAM_STATE_SELECT; current_param_page = 0;
    sequencer_init();
}

ui_state_t ui_state_get(void) { return current_state; }
uint8_t ui_state_get_base_octave(void) { return base_octave_note; }
chord_type_t ui_state_get_chord_type(void) { return (current_state == UI_STATE_SEQ_EDIT) ? (chord_type_t)seq_edit_octave : current_chord_type; }
param_state_t ui_state_get_param_state(void) { return current_param_state; }
uint8_t ui_state_get_param_page(void) { return current_param_page; }
uint8_t ui_state_get_last_param_value(void) { return last_param_value; }
uint32_t ui_state_get_param_timer(void) { return param_display_timer; }
void ui_state_update_timers(void) { if (param_display_timer > 0) param_display_timer--; }

void ui_state_process_buttons(uint32_t button_states) {
    uint32_t changed = button_states ^ previous_button_states;
    uint32_t pressed = changed & button_states;
    sequencer_t *seq = sequencer_get();

#if CURRENT_BOARD == BOARD_OMSK
    // OMSK logic omitted for brevity in this scratch, but it's maintained in final
    // (Focusing on Nizkoteno as requested)
#elif CURRENT_BOARD == BOARD_NIZKOTENO
    bool f1_pressed = is_pad_pressed(button_states, BTN_PAD_9);
    bool f2_pressed = is_pad_pressed(button_states, BTN_PAD_10);

    switch (current_state) {
        case UI_STATE_PIANO:
            if (!f1_pressed && is_pad_just_pressed(pressed, BTN_PAD_8)) {
                base_octave_note += 12; if (base_octave_note > 108) base_octave_note = 24;
            }
            if (f1_pressed) {
                // F1 held: chord selection
                for (int i = 0; i < 8; i++) if (is_pad_just_pressed(pressed, (logical_button_t)i)) current_chord_type = (chord_type_t)i;
            } else {
                // Normal piano play
                const uint8_t scale[7] = {0, 2, 4, 5, 7, 9, 11};
                for (int i = 0; i < 7; i++) {
                    logical_button_t pad = (logical_button_t)i;
                    if (is_pad_just_pressed(pressed, pad)) handleNoteOn(g_midi_ch, base_octave_note + scale[i], 100);
                    if (is_pad_just_released(changed, button_states, pad)) handleNoteOff(g_midi_ch, base_octave_note + scale[i], 0);
                }
            }
            // F1+F2 = go to SEQ
            if (f1_pressed && is_pad_just_pressed(pressed, BTN_PAD_10)) {
                current_state = UI_STATE_SEQ;
            }
            // F2 alone = go to PARAMS
            if (!f1_pressed && is_pad_just_pressed(pressed, BTN_PAD_10)) {
                current_state = UI_STATE_PARAMS;
                current_param_state = PARAM_STATE_SELECT;
            }
            break;

        case UI_STATE_SEQ: {
            static bool page_combo_used = false;
            if (is_pad_just_pressed(pressed, BTN_PAD_10)) {
                page_combo_used = false;
            }

            if (f1_pressed && f2_pressed) {
                // F1+F2 = play/pause (only on new press)
                if (is_pad_just_pressed(pressed, BTN_PAD_9) || is_pad_just_pressed(pressed, BTN_PAD_10)) {
                    sequencer_toggle_play();
                }
            } else if (f1_pressed) {
                // F1 + Pad = edit step
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        editing_step_idx = seq->current_page * 8 + i;
                        editing_note_idx = 0;
                        current_state = UI_STATE_SEQ_EDIT;
                        // Do not clear the step here to retain existing notes!
                    }
                }
            } else if (f2_pressed) {
                // F2 + Pad = set stop step
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        sequencer_set_stop_step(seq->current_page * 8 + i);
                        page_combo_used = true;
                    }
                }
            }
            
            // F2 released alone (no pad) = switch page (works on pause too)
            if (is_pad_just_released(changed, button_states, BTN_PAD_10) && !page_combo_used && !f1_pressed) {
                sequencer_next_page();
            }

            if (!f1_pressed && !f2_pressed) {
                // No modifier: mute/unmute steps
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        uint8_t step_idx = seq->current_page * 8 + i;
                        if (seq->steps[step_idx].num_notes == 0) {
                            // Empty step: always go to edit
                            editing_step_idx = step_idx;
                            editing_note_idx = 0;
                            current_state = UI_STATE_SEQ_EDIT;
                        } else {
                            // Step has notes: toggle mute
                            sequencer_toggle_mute(step_idx);
                        }
                    }
                }
                // Btn9 = back to Piano (stops playback)
                if (is_pad_just_pressed(pressed, BTN_PAD_9)) {
                    current_state = UI_STATE_PIANO;
                    seq->playing = false;
                }
            }
            break;
        }

        case UI_STATE_SEQ_EDIT: {
            bool done_pressed = is_pad_just_pressed(pressed, BTN_PAD_9);
            bool page_held = is_pad_pressed(button_states, BTN_PAD_10);
            static bool edit_page_combo_used = false;
            static uint8_t playing_note = 255;

            if (is_pad_just_pressed(pressed, BTN_PAD_10)) {
                edit_page_combo_used = false;
            }

            if (done_pressed || current_state != UI_STATE_SEQ_EDIT) { 
                if (playing_note != 255) {
                    handleNoteOff(g_midi_ch, playing_note, 0);
                    playing_note = 255;
                }
                // If no notes added, keep it muted
                if (seq->steps[editing_step_idx].num_notes == 0) {
                    seq->steps[editing_step_idx].muted = true;
                }
                current_state = UI_STATE_SEQ; 
                break; 
            }
            
            if (is_pad_just_released(changed, button_states, BTN_PAD_10) && !edit_page_combo_used) {
                editing_note_idx = (editing_note_idx + 1) % 4;
                if (playing_note != 255) {
                    handleNoteOff(g_midi_ch, playing_note, 0);
                    playing_note = 255;
                }
            }
            
            if (page_held) {
                // Chord selection in step edit
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        sequencer_set_step_chord(editing_step_idx, (chord_type_t)i);
                        edit_page_combo_used = true;
                    }
                }
            } else {
                const uint8_t scale[7] = {0, 2, 4, 5, 7, 9, 11};
                uint8_t note_to_set = 255;
                
                // Priority 1: Sharps (dual press)
                if (is_pad_pressed(button_states, BTN_PAD_1) && is_pad_pressed(button_states, BTN_PAD_2)) note_to_set = seq_edit_octave + 1;
                else if (is_pad_pressed(button_states, BTN_PAD_2) && is_pad_pressed(button_states, BTN_PAD_3)) note_to_set = seq_edit_octave + 3;
                else if (is_pad_pressed(button_states, BTN_PAD_4) && is_pad_pressed(button_states, BTN_PAD_5)) note_to_set = seq_edit_octave + 6;
                else if (is_pad_pressed(button_states, BTN_PAD_5) && is_pad_pressed(button_states, BTN_PAD_6)) note_to_set = seq_edit_octave + 8;
                else if (is_pad_pressed(button_states, BTN_PAD_6) && is_pad_pressed(button_states, BTN_PAD_7)) note_to_set = seq_edit_octave + 10;
                else {
                    // Priority 2: Single notes
                    for (int i = 0; i < 7; i++) {
                        if (is_pad_just_pressed(pressed, (logical_button_t)i)) note_to_set = seq_edit_octave + scale[i];
                    }
                }

                if (note_to_set != 255) {
                    if (seq->steps[editing_step_idx].num_notes > editing_note_idx && seq->steps[editing_step_idx].notes[editing_note_idx] == note_to_set) {
                        // Toggle OFF: note is already there
                        sequencer_remove_note(editing_step_idx, editing_note_idx);
                        if (playing_note != 255) handleNoteOff(g_midi_ch, playing_note, 0);
                        playing_note = 255;
                    } else {
                        // Toggle ON / Change:
                        sequencer_set_note(editing_step_idx, editing_note_idx, note_to_set);
                        seq->steps[editing_step_idx].muted = false; // Unmute step when note is added
                        if (playing_note != 255) handleNoteOff(g_midi_ch, playing_note, 0);
                        handleNoteOn(g_midi_ch, note_to_set, 100);
                        playing_note = note_to_set;
                    }
                }
                
                // Turn off note when no pads are pressed
                if (playing_note != 255 && (button_states & 0xFF) == 0) {
                    handleNoteOff(g_midi_ch, playing_note, 0);
                    playing_note = 255;
                }
            }

            if (is_pad_just_pressed(pressed, BTN_PAD_8)) {
                seq_edit_octave += 12; if (seq_edit_octave > 108) seq_edit_octave = 24;
            }
            break;
        }

        case UI_STATE_PARAMS:
            if (current_param_state == PARAM_STATE_SELECT) {
                // Group selection 1-8
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) { 
                        current_param_state = (param_state_t)(PARAM_STATE_OSC + i); 
                        current_param_page = 0; 
                        printf("Param Group: %d\n", i);
                    }
                }
                // Toggle back to Piano or Seq
                if (is_pad_just_pressed(pressed, BTN_PAD_9)) {
                    current_state = UI_STATE_PIANO;
                }
                if (is_pad_just_pressed(pressed, BTN_PAD_10)) {
                    current_state = UI_STATE_SEQ;
                }
            } else {
                // Inside a parameter group
                if (is_pad_just_pressed(pressed, BTN_PAD_9)) {
                    current_param_state = PARAM_STATE_SELECT;
                }
                if (is_pad_just_pressed(pressed, BTN_PAD_10)) {
                    current_param_page = (current_param_page + 1) % 2;
                }
                
                // Adjust parameters with pads 1-8
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        uint8_t cc = 255; 
                        int8_t delta = (i < 4) ? 5 : -5; // Top row plus, bottom row minus (simple logic for now)
                        
                        // Mapping logic based on group and page
                        switch(current_param_state) {
                            case PARAM_STATE_OSC:
                                if (current_param_page == 0) {
                                    if (i == 0 || i == 4) cc = 10; // Example: Osc 1 Wave
                                    if (i == 1 || i == 5) cc = 11; // Example: Osc 1 Shape
                                }
                                break;
                            case PARAM_STATE_FILT:
                                if (current_param_page == 0) {
                                    if (i == 0 || i == 4) cc = 20; // Cutoff
                                    if (i == 1 || i == 5) cc = 21; // Resonance
                                }
                                break;
                            case PARAM_STATE_MISC: 
                                if (current_param_page == 0 && (i == 0 || i == 4)) { 
                                    cc = 254; // Virtual BPM
                                    delta = (i == 0 ? 1 : -1); 
                                } 
                                break;
                            default: break;
                        }
                        if (cc != 255) adjust_parameter(cc, delta);
                    }
                }
            } break;
        default: break;
    }
#endif
    previous_button_states = button_states;
}
