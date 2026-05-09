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
    sequencer_set_bpm(g_seq.bpm);
}

void sequencer_set_bpm(uint8_t value) {
    g_seq.bpm = value;
    uint32_t bpm_real = 50 + value;
    // Step duration for 16th notes: (60 / BPM) / 4 in seconds
    // In microseconds: (15,000,000 / BPM)
    g_seq.step_duration_us = 15000000 / bpm_real;
}

sequencer_t* sequencer_get(void) {
    return &g_seq;
}

void sequencer_toggle_play(void) {
    g_seq.playing = !g_seq.playing;
    if (g_seq.playing) {
        g_seq.last_step_time_us = time_us_32();
    } else {
        // Turn off all notes when stopping
        for (int i = 0; i < 128; i++) handleNoteOff(g_midi_ch, i, 0);
    }
}

void sequencer_update(uint32_t now_us) {
    if (!g_seq.playing) return;

    if (now_us - g_seq.last_step_time_us >= g_seq.step_duration_us) {
        // Stop current notes
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

        // Play new notes
        seq_step_t *curr_step = &g_seq.steps[g_seq.current_step];
        if (!curr_step->muted && curr_step->num_notes > 0) {
            for (int i = 0; i < curr_step->num_notes; i++) {
                handleNoteOn(g_midi_ch, curr_step->notes[i], 100);
            }
        }

        g_seq.last_step_time_us = now_us;
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
