#include "sequencer.h"
#include "pico/stdlib.h"
#include "pra32-u-synth.h"

extern void handleNoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity);
extern void handleNoteOff(uint8_t channel, uint8_t pitch, uint8_t velocity);
extern uint8_t g_midi_ch;

static sequencer_t g_seq;

void sequencer_init(void) {
    for (int i = 0; i < SEQ_MAX_STEPS; i++) {
        g_seq.steps[i].num_notes = 0;
        g_seq.steps[i].muted = false;
        g_seq.steps[i].is_stop_step = false;
        g_seq.steps[i].chord = CHORD_OFF;
    }
    g_seq.current_step = 0;
    g_seq.stop_step_idx = SEQ_MAX_STEPS - 1;
    g_seq.bpm = 120 - 50; // Default 120 BPM
    g_seq.playing = false;
    g_seq.current_page = 0;
    g_seq.last_step_time_us = 0;
    g_seq.next_gate_off_us = 0;
    g_seq.division = 4;     // Default: Standard (16 steps per measure)
    g_seq.gate_length = 64; // Default: 50%
    g_seq.swing = 0;        // Default: no swing (50%)
    sequencer_set_bpm(120 - 50); // CC 70 for 120 BPM
}

void sequencer_set_bpm(uint8_t value) {
    g_seq.bpm = value;
    uint32_t bpm_real = 50 + value;
    // speeds = {1/16, 1/8, 1/4, 1/2, 1, 2, 4}
    // mapped to beats per step (Standard = 0.25 beats = 1/16 note):
    // 1/16: 16x slower (4.0 beats = 1 measure)
    // 1/8:  8x slower  (2.0 beats = 1/2 measure)
    // 1/4:  4x slower  (1.0 beats = 1/4 measure)
    // 1/2:  2x slower  (0.5 beats = 1/8 note)
    // 1:    Standard   (0.25 beats = 1/16 note)
    // 2:    2x faster  (0.125 beats = 1/32 note)
    // 4:    4x faster  (0.0625 beats = 1/64 note)
    static const float speeds[] = {4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f, 0.0625f};
    uint8_t speed_idx = g_seq.division; 
    if (speed_idx > 6) speed_idx = 6;

    // duration = (60 / BPM) * speed_in_beats
    g_seq.step_duration_us = (uint32_t)((60000000.0f / bpm_real) * speeds[speed_idx]);
}

sequencer_t* sequencer_get(void) {
    return &g_seq;
}

void sequencer_toggle_play(void) {
    g_seq.playing = !g_seq.playing;
    if (g_seq.playing) {
        g_seq.last_step_time_us = time_us_32();
        g_seq.next_gate_off_us = 0;
    } else {
        // Turn off all notes when stopping
        for (int i = 0; i < 128; i++) handleNoteOff(g_midi_ch, i, 0);
    }
}

void sequencer_update(uint32_t now_us) {
    if (!g_seq.playing) return;

    // Handle Gate Off
    if (g_seq.next_gate_off_us != 0 && now_us >= g_seq.next_gate_off_us) {
        seq_step_t *curr_step = &g_seq.steps[g_seq.current_step];
        for (int i = 0; i < curr_step->num_notes; i++) {
            handleNoteOff(g_midi_ch, curr_step->notes[i], 0);
        }
        g_seq.next_gate_off_us = 0;
    }

    if (now_us - g_seq.last_step_time_us >= g_seq.step_duration_us) {
        // Stop current notes (if not already turned off by gate)
        seq_step_t *prev_step = &g_seq.steps[g_seq.current_step];
        for (int i = 0; i < prev_step->num_notes; i++) {
            handleNoteOff(g_midi_ch, prev_step->notes[i], 0);
        }

        // Advance step
        uint8_t next_step = g_seq.current_step + 1;
        if (next_step >= SEQ_MAX_STEPS || g_seq.steps[next_step].is_stop_step || next_step > g_seq.stop_step_idx) {
            g_seq.current_step = 0;
        } else {
            g_seq.current_step = next_step;
        }

        // Auto-switch page in play mode to follow the playhead
        g_seq.current_page = g_seq.current_step / SEQ_STEPS_PER_PAGE;

        // Apply Swing: only for odd steps (1, 3, 5...)
        // Step duration is adjusted based on swing.
        // g_seq.swing is 0-127, mapping to 50%-75% of the beat pair.
        // Swing factor S = (swing / 127) * 0.25 + 0.5
        // First step duration: D1 = 2 * D * S
        // Second step duration: D2 = 2 * D * (1-S)
        // Wait, it's easier: if step is odd (1, 3, 5), it was delayed.
        // The delay is (S - 0.5) * (2 * D).
        uint32_t base_duration = g_seq.step_duration_us;
        uint32_t actual_duration = base_duration;
        
        if (g_seq.swing > 0) {
            // S = 0.5 + (swing/127 * 0.25)
            // If current step is even (0, 2, 4...), the time to next step is D * 2 * S
            // If current step is odd (1, 3, 5...), the time to next step is D * 2 * (1 - S)
            float s = 0.5f + ((float)g_seq.swing / 127.0f) * 0.25f;
            if ((g_seq.current_step % 2) == 0) {
                actual_duration = (uint32_t)(base_duration * 2.0f * s);
            } else {
                actual_duration = (uint32_t)(base_duration * 2.0f * (1.0f - s));
            }
        }

        // Play new notes
        seq_step_t *curr_step = &g_seq.steps[g_seq.current_step];
        if (!curr_step->muted && curr_step->num_notes > 0) {
            for (int i = 0; i < curr_step->num_notes; i++) {
                handleNoteOn(g_midi_ch, curr_step->notes[i], 100);
            }
            // Set next gate off
            if (g_seq.gate_length < 127) {
                g_seq.next_gate_off_us = now_us + (uint32_t)(base_duration * ((float)g_seq.gate_length / 127.0f));
            } else {
                g_seq.next_gate_off_us = 0; // Legato
            }
        } else {
            g_seq.next_gate_off_us = 0;
        }

        g_seq.last_step_time_us = now_us;
        g_seq.step_duration_us = actual_duration; // Store for next comparison
        // But wait, step_duration_us is used at line 55. If I change it here, it affects the *next* step.
    }
}

void sequencer_toggle_mute(uint8_t step_idx) {
    if (step_idx < SEQ_MAX_STEPS) {
        g_seq.steps[step_idx].muted = !g_seq.steps[step_idx].muted;
    }
}

void sequencer_set_stop_step(uint8_t step_idx) {
    if (step_idx < SEQ_MAX_STEPS) {
        // Clear previous stop step
        for(int i=0; i<SEQ_MAX_STEPS; i++) g_seq.steps[i].is_stop_step = false;
        g_seq.stop_step_idx = step_idx;
        g_seq.steps[step_idx].is_stop_step = true;
    }
}

void sequencer_add_note(uint8_t step_idx, uint8_t note) {
    if (step_idx < SEQ_MAX_STEPS) {
        seq_step_t *s = &g_seq.steps[step_idx];
        if (s->num_notes < SEQ_MAX_NOTES_PER_STEP) {
            s->notes[s->num_notes++] = note;
        }
    }
}

void sequencer_set_note(uint8_t step_idx, uint8_t note_idx, uint8_t note) {
    if (step_idx < SEQ_MAX_STEPS && note_idx < SEQ_MAX_NOTES_PER_STEP) {
        seq_step_t *s = &g_seq.steps[step_idx];
        s->notes[note_idx] = note;
        if (note_idx >= s->num_notes) s->num_notes = note_idx + 1;
    }
}

void sequencer_remove_note(uint8_t step_idx, uint8_t note_idx) {
    if (step_idx < SEQ_MAX_STEPS && note_idx < SEQ_MAX_NOTES_PER_STEP) {
        seq_step_t *s = &g_seq.steps[step_idx];
        s->notes[note_idx] = 0;
        // If it was the last note, we could potentially reduce num_notes, 
        // but for slots, we just mark it as empty (0).
        // If the slot 0 is removed, we treat it as empty.
        if (note_idx == 0 && s->num_notes == 1) s->num_notes = 0;
    }
}
void sequencer_set_step_chord(uint8_t step_idx, chord_type_t chord) {
    if (step_idx < SEQ_MAX_STEPS) {
        g_seq.steps[step_idx].chord = chord;
    }
}

void sequencer_clear_step(uint8_t step_idx) {
    if (step_idx < SEQ_MAX_STEPS) {
        g_seq.steps[step_idx].num_notes = 0;
    }
}

void sequencer_next_page(void) {
    g_seq.current_page = (g_seq.current_page + 1) % SEQ_PAGES;
}

#include "hardware/flash.h"
#include "hardware/sync.h"
#include <string.h>

#define SEQ_FLASH_MAGIC 0x51455350 // 'PSEQ'

typedef struct {
    uint32_t magic;
    sequencer_t data;
} seq_flash_slot_t;

// Standard Pico flash size fallback
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

// We use the very last sector of flash memory
#define SEQ_FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define SEQ_FLASH_ADDR ((const uint8_t *)(XIP_BASE + SEQ_FLASH_TARGET_OFFSET))

void sequencer_save(uint8_t slot) {
    if (slot >= 8) return;

    // We must read the whole sector into RAM, modify the slot, erase sector, program sector
    static uint8_t sector_buffer[FLASH_SECTOR_SIZE];
    
    // Copy current flash contents to buffer
    memcpy(sector_buffer, SEQ_FLASH_ADDR, FLASH_SECTOR_SIZE);

    seq_flash_slot_t *slots = (seq_flash_slot_t *)sector_buffer;

    // Update target slot
    slots[slot].magic = SEQ_FLASH_MAGIC;
    memcpy(&slots[slot].data, &g_seq, sizeof(sequencer_t));

    // Erase and Program (Interrupts must be disabled)
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(SEQ_FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    flash_range_program(SEQ_FLASH_TARGET_OFFSET, sector_buffer, FLASH_SECTOR_SIZE);
    restore_interrupts(ints);
}

bool sequencer_load(uint8_t slot) {
    if (slot >= 8) return false;

    const seq_flash_slot_t *flash_slots = (const seq_flash_slot_t *)SEQ_FLASH_ADDR;
    
    if (flash_slots[slot].magic == SEQ_FLASH_MAGIC) {
        memcpy(&g_seq, &flash_slots[slot].data, sizeof(sequencer_t));
        g_seq.playing = false; // Never resume playing automatically
        return true;
    }
    
    return false;
}
