#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ui_types.h"
#include "config.h"

extern uint8_t g_midi_ch;

#define SEQ_MAX_STEPS 32
#define SEQ_STEPS_PER_PAGE 8
#define SEQ_PAGES 4
#define SEQ_MAX_NOTES_PER_STEP 4

typedef struct {
    uint8_t notes[SEQ_MAX_NOTES_PER_STEP];
    uint8_t num_notes;
    bool muted;
    bool is_stop_step;
    chord_type_t chord;
} seq_step_t;

typedef struct {
    seq_step_t steps[SEQ_MAX_STEPS];
    uint8_t current_step;
    uint8_t stop_step_idx; 
    uint8_t bpm;           
    bool playing;
    uint8_t current_page;
    uint32_t last_step_time_us;
    uint32_t step_duration_us;
    uint32_t next_gate_off_us;
    uint8_t division;    // 1-16 (steps per measure)
    uint8_t gate_length; // 0-127 (note length within step)
    uint8_t swing;       // 0-127 (swing amount, 0=50%, 127=75%)
} sequencer_t;

void sequencer_init(void);
void sequencer_update(uint32_t now_us);
sequencer_t* sequencer_get(void);
void sequencer_set_note(uint8_t step_idx, uint8_t note_idx, uint8_t note);
void sequencer_remove_note(uint8_t step_idx, uint8_t note_idx);
void sequencer_set_step_chord(uint8_t step_idx, chord_type_t chord);
void sequencer_clear_step(uint8_t step_idx);
void sequencer_toggle_mute(uint8_t step_idx);
void sequencer_set_stop_step(uint8_t step_idx);
void sequencer_next_page(void);
void sequencer_toggle_play(void);
void sequencer_set_bpm(uint8_t bpm);

void sequencer_toggle_play(void);
void sequencer_set_bpm(uint8_t value);
void sequencer_toggle_mute(uint8_t step_idx);
void sequencer_set_stop_step(uint8_t step_idx);
void sequencer_add_note(uint8_t step_idx, uint8_t note);
void sequencer_clear_step(uint8_t step_idx);
void sequencer_next_page(void);

// Save current sequence to slot (0-7)
void sequencer_save(uint8_t slot);

// Load sequence from slot (0-7). Returns true if loaded, false if empty.
bool sequencer_load(uint8_t slot);
