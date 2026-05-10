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
static bool piano_hold_active = false;
static uint8_t piano_hold_note = 255;

// Sequencer edit state
uint8_t editing_step_idx = 0;
uint8_t editing_note_idx = 0; // Current note slot (0-3)
uint8_t seq_edit_octave = 60;
static bool seq_edit_ignore_done_hold = false;
static bool seq_page_combo_used = false;
static bool seq_param_combo_used = false;
static bool seq_edit_page_combo_used = false;
static bool seq_edit_done_combo_used = false;

bool ui_state_seq_edit_is_done_held(uint32_t button_states) {
    return (button_states & (1 << BTN_PAD_9)) != 0 && !seq_edit_ignore_done_hold;
}

// ---------------------------------------------------------------------------
// Stepped thresholds for discrete parameters
// ---------------------------------------------------------------------------
static const uint8_t thr_2[]  = {63, 127};                       // 2-step
static const uint8_t thr_3[]  = {31, 95, 127};                   // 3-step (keytrk)
static const uint8_t thr_6[]  = {12, 38, 63, 88, 114, 127};      // 6-step wave
static const uint8_t thr_gate[] = {0, 6, 13, 19, 25, 32, 38, 44, 51, 57, 63, 70, 76, 82, 89, 95, 101, 108, 114, 120, 127}; // 21-step (5%)
static const uint8_t thr_swing[] = {0, 5, 10, 15, 20, 25, 30, 35, 41, 46, 51, 56, 61, 66, 71, 76, 81, 86, 91, 96, 102, 107, 112, 117, 122, 127}; // 26-step (1%, 50-75%)
static const uint8_t thr_speed[] = {17, 35, 53, 71, 89, 107, 127}; // 7-step (1/16 to 4 notes)
static const uint8_t thr_pb[] = {2, 6, 10, 14, 18, 22, 26, 30,  // pitch bend range
                                   34, 38, 42, 46, 50, 54, 58,
                                   62, 66, 70, 74, 78, 82, 86,
                                   90, 94, 98, 102, 106, 110,
                                   114, 118, 122, 127};

// ---------------------------------------------------------------------------
// Virtual CC codes (above MIDI range, handled specially)
// ---------------------------------------------------------------------------
#define VCC_PRESET    250   // Program Change +
#define VCC_NONE      255   // No assignment (X)

// ---------------------------------------------------------------------------
// Parameter descriptor
//   cc        – MIDI CC (or VCC_*)
//   delta     – step size for continuous params; 0 = cycle (wrap-around)
//   steps     – pointer to threshold array for stepped params (nullptr if continuous)
//   num_steps – length of steps array
// ---------------------------------------------------------------------------
struct ParamEntry {
    uint8_t         cc;
    int8_t          delta;
    const uint8_t*  steps;
    uint8_t         num_steps;
};

// Convenience macros
#define CONT(cc_, d)          {cc_, d,  nullptr, 0}
#define STEP(cc_, arr)        {cc_, 1,  arr, (uint8_t)(sizeof(arr)/sizeof(arr[0]))}
#define CYCLE(cc_, arr)       {cc_, 0,  arr, (uint8_t)(sizeof(arr)/sizeof(arr[0]))}
#define NONE                  {VCC_NONE, 0, nullptr, 0}
#define PRESET_ENTRY          {VCC_PRESET, 1, nullptr, 0}

// ---------------------------------------------------------------------------
// Full parameter table  [group][page][pad 0..7]
//   Pads 0-3 = top row  (+direction for continuous/stepped)
//   Pads 4-7 = bottom row (-direction for continuous/stepped)
//   For CYCLE (~) params: delta=0, sign ignored — always steps forward
//   groups: 0=OSC 1=SEQ 2=FILT 3=EG 4=LFO 5=AMP 6=FX 7=MISC
// ---------------------------------------------------------------------------
static const ParamEntry param_table[8][2][8] = {
    // --- 0: OSC ---
    {{
        // top row (pad 0-3): +
        STEP(OSC_1_WAVE,    thr_6), // osc1.wave+
        CONT(OSC_1_SHAPE,   8),     // osc1.shape+
        CONT(OSC_1_MORPH,   8),     // osc1.morph+
        CONT(MIXER_SUB_OSC, 8),     // mix.noise_sub+
        // bottom row (pad 4-7): -
        STEP(OSC_1_WAVE,    thr_6), // osc1.wave-
        CONT(OSC_1_SHAPE,   8),     // osc1.shape-
        CONT(OSC_1_MORPH,   8),     // osc1.morph-
        CONT(MIXER_SUB_OSC, 8),     // mix.noise_sub-
    },{
        STEP(OSC_2_WAVE,    thr_6), // osc2.wave+
        CONT(OSC_2_COARSE,  8),     // osc2.coarse+
        CONT(OSC_2_PITCH,   8),     // osc2.pitch+
        CONT(MIXER_OSC_MIX, 8),     // mix.osc1_2+
        STEP(OSC_2_WAVE,    thr_6), // osc2.wave-
        CONT(OSC_2_COARSE,  8),     // osc2.coarse-
        CONT(OSC_2_PITCH,   8),     // osc2.pitch-
        CONT(MIXER_OSC_MIX, 8),     // mix.osc1_2-
    }},
    // --- 1: SEQ ---
    {{
        CONT(SEQ_CC_BPM,   1),  // bpm+
        STEP(SEQ_CC_DIV,   thr_speed), // speed+
        STEP(SEQ_CC_GATE,  thr_gate),  // gateLength+
        STEP(SEQ_CC_SWING, thr_swing), // swing+
        CONT(SEQ_CC_BPM,   1),  // bpm-
        STEP(SEQ_CC_DIV,   thr_speed), // speed-
        STEP(SEQ_CC_GATE,  thr_gate),  // gateLength-
        STEP(SEQ_CC_SWING, thr_swing), // swing-
    },{
        NONE, NONE, NONE, NONE,
        NONE, NONE, NONE, NONE,
    }},
    // --- 2: FILT ---
    {{
        CONT(FILTER_CUTOFF,   8),    // filt.cutoff+
        CONT(FILTER_RESO,     8),    // filt.res+
        CONT(FILTER_EG_AMT,   8),    // eg.filtAMT+
        CONT(LFO_FILTER_AMT,  8),    // lfo.filtAMT+
        CONT(FILTER_CUTOFF,   8),    // filt.cutoff-
        CONT(FILTER_RESO,     8),    // filt.res-
        CONT(FILTER_EG_AMT,   8),    // eg.filtAMT-
        CONT(LFO_FILTER_AMT,  8),    // lfo.filtAMT-
    },{
        STEP(FILTER_KEY_TRK,  thr_3),// filt.keytrk+
        CYCLE(FILTER_MODE,    thr_2),// filt.mode~
        CONT(BTH_FILTER_AMT,  8),    // breath.filtAMT+
        NONE,                        // x
        STEP(FILTER_KEY_TRK,  thr_3),// filt.keytrk-
        CYCLE(FILTER_MODE,    thr_2),// filt.mode~ (both rows cycle)
        CONT(BTH_FILTER_AMT,  8),    // breath.filtAMT-
        NONE,                        // x
    }},
    // --- 3: EG ---
    {{
        CONT(EG_ATTACK,   8),        // eg.attk+
        CONT(EG_DECAY,    8),        // eg.decay+
        CONT(EG_SUSTAIN,  8),        // eg.sus+
        CONT(EG_RELEASE,  8),        // eg.rel+
        CONT(EG_ATTACK,   8),        // eg.attk-
        CONT(EG_DECAY,    8),        // eg.decay-
        CONT(EG_SUSTAIN,  8),        // eg.sus-
        CONT(EG_RELEASE,  8),        // eg.rel-
    },{
        // NOTE: pad1=eg.oscDst~, pad5=eg.ampMod~ — different params same column!
        CONT(EG_VEL_SENS,   8),      // eg.VelSens+
        CYCLE(EG_OSC_DST,   thr_3),  // eg.oscDst~  (top)
        CONT(EG_OSC_AMT,    8),      // eg.oscAMT+
        CONT(FILTER_EG_AMT, 8),      // eg.filtAMT+
        CONT(EG_VEL_SENS,   8),      // eg.VelSens-
        CYCLE(EG_AMP_MOD,   thr_2),  // eg.ampMod~  (bottom — different!)
        CONT(EG_OSC_AMT,    8),      // eg.oscAMT-
        CONT(FILTER_EG_AMT, 8),      // eg.filtAMT-
    }},
    // --- 4: LFO ---
    {{
        // lfo.wave~ on both rows (same cycle param)
        CYCLE(LFO_WAVE,     thr_6),  // lfo.wave~ (top)
        CONT(LFO_RATE,      8),      // lfo.rate+
        CONT(LFO_DEPTH,     8),      // lfo.depth+
        CONT(LFO_FADE_TIME, 8),      // lfo.fadeTime+
        CYCLE(LFO_WAVE,     thr_6),  // lfo.wave~ (bottom — same cycle)
        CONT(LFO_RATE,      8),      // lfo.rate-
        CONT(LFO_DEPTH,     8),      // lfo.depth-
        CONT(LFO_FADE_TIME, 8),      // lfo.fadeTime-
    },{
        NONE,                        // x
        CYCLE(LFO_OSC_DST,  thr_3),  // lfo.oscDst~
        CONT(LFO_OSC_AMT,   8),      // lfo.oscAMT+
        CONT(LFO_FILTER_AMT,8),      // lfo.filtAMT+
        NONE,                        // x
        CYCLE(LFO_OSC_DST,  thr_3),  // lfo.oscDst~ (same cycle both rows)
        CONT(LFO_OSC_AMT,   8),      // lfo.oscAMT-
        CONT(LFO_FILTER_AMT,8),      // lfo.filtAMT-
    }},
    // --- 5: AMP ---
    {{
        CONT(AMP_ATTACK,  8),        // amp.attk+
        CONT(AMP_DECAY,   8),        // amp.decay+
        CONT(AMP_SUSTAIN, 8),        // amp.sus+
        CONT(AMP_RELEASE, 8),        // amp.rel+
        CONT(AMP_ATTACK,  8),        // amp.attk-
        CONT(AMP_DECAY,   8),        // amp.decay-
        CONT(AMP_SUSTAIN, 8),        // amp.sus-
        CONT(AMP_RELEASE, 8),        // amp.rel-
    },{
        // NOTE: pad2=breath.ampMod~, pad6=x — top has param, bottom doesn't!
        CONT(AMP_VEL_SENS,    8),    // amp.VelSens+
        CONT(AMP_GAIN,        8),    // amp.gain+
        CYCLE(BTH_AMP_MOD,    thr_3),// breath.ampMod~ (top only)
        NONE,                        // x
        CONT(AMP_VEL_SENS,    8),    // amp.VelSens-
        CONT(AMP_GAIN,        8),    // amp.gain-
        NONE,                        // x (bottom has no param for col C)
        NONE,                        // x
    }},
    // --- 6: FX ---
    {{
        // NOTE: pad2=delay.mode~, pad6=x
        CONT(DELAY_FEEDBACK, 8),     // delay.fb+
        CONT(DELAY_TIME,     8),     // delay.time+
        CYCLE(DELAY_MODE,    thr_2), // delay.mode~ (top only)
        NONE,                        // x
        CONT(DELAY_FEEDBACK, 8),     // delay.fb-
        CONT(DELAY_TIME,     8),     // delay.time-
        NONE,                        // x (bottom — no mode on minus row)
        NONE,                        // x
    },{
        CONT(CHORUS_RATE,  8),       // chorus.rate+
        CONT(CHORUS_DEPTH, 8),       // chorus.depth+
        CONT(CHORUS_MIX,   8),       // chorus.mix+
        NONE,                        // x
        CONT(CHORUS_RATE,  8),       // chorus.rate-
        CONT(CHORUS_DEPTH, 8),       // chorus.depth-
        CONT(CHORUS_MIX,   8),       // chorus.mix-
        NONE,                        // x
    }},
    // --- 7: MISC ---
    {{
        // NOTE: pad1=voice.mode~, pad5=voice.assignMode~ — different params!
        CONT(PORTAMENTO,   8),           // portamento+
        CYCLE(VOICE_MODE,  thr_6),       // voice.mode~   (top)
        STEP(P_BEND_RANGE, thr_pb),      // pitchBend.range+
        PRESET_ENTRY,                    // preset+
        CONT(PORTAMENTO,   8),           // portamento-
        CYCLE(VOICE_ASGN_MODE, thr_2),   // voice.assignMode~ (bottom — different!)
        STEP(P_BEND_RANGE, thr_pb),      // pitchBend.range-
        PRESET_ENTRY,                    // preset-
    },{
        NONE, NONE, NONE, NONE,
        NONE, NONE, NONE, NONE,
    }},
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static inline bool is_pad_pressed(uint32_t states, logical_button_t pad)      { return (states & (1 << pad)) != 0; }
static inline bool is_pad_just_pressed(uint32_t pressed, logical_button_t pad) { return (pressed & (1 << pad)) != 0; }
static inline bool is_pad_just_released(uint32_t changed, uint32_t states, logical_button_t pad) {
    return (changed & (1 << pad)) != 0 && !is_pad_pressed(states, pad);
}
static inline bool pad_is_plus(int i) { return i < 4; }
static inline int pad_to_col(int i) { return i % 4; }

// ---------------------------------------------------------------------------
// Adjust a single parameter entry
// ---------------------------------------------------------------------------
static void adjust_entry(const ParamEntry& e, int8_t sign) {
    // sign: +1 for plus buttons, -1 for minus buttons
    uint8_t cc = e.cc;

    // --- Virtual / Special CCs ---
    if (cc == VCC_NONE) return;

    if (cc == VCC_PRESET) {
        current_preset = (uint8_t)((int)current_preset + sign);
        if (current_preset > PRESET_PROGRAM_NUMBER_MAX) current_preset = 0;
        if (current_preset > 127) current_preset = PRESET_PROGRAM_NUMBER_MAX; // underflow
        handleProgramChange(g_midi_ch, current_preset);
        last_param_value = (uint8_t)((current_preset * 127) / PRESET_PROGRAM_NUMBER_MAX);
        param_display_timer = 66;
        return;
    }

    if (cc == SEQ_CC_BPM) {
        int val = (int)sequencer_get()->bpm + sign;
        if (val > 127) val = 127; if (val < 0) val = 0;
        sequencer_set_bpm((uint8_t)val);
        handleControlChange(g_midi_ch, cc, (uint8_t)val);
        last_param_value = (uint8_t)val;
        param_display_timer = 66;
        return;
    }

    if (cc == SEQ_CC_DIV) {
        uint8_t current = getCurrentControllerValue(g_midi_ch, cc);
        uint8_t next = current;
        int cur_idx = 6;
        for (int i = 0; i < 7; i++) {
            if (current <= thr_speed[i]) { cur_idx = i; break; }
        }

        int next_idx = cur_idx + sign;
        if (next_idx > 6) next_idx = 6; if (next_idx < 0) next_idx = 0;
        next = thr_speed[next_idx];

        sequencer_get()->division = (uint8_t)next_idx;
        sequencer_set_bpm(sequencer_get()->bpm); // Recalculate duration
        
        handleControlChange(g_midi_ch, cc, next);
        last_param_value = next;
        param_display_timer = 66;
        return;
    }

    if (cc == SEQ_CC_GATE) {
        // Handled by STEP logic below
    }

    if (cc == SEQ_CC_SWING) {
        // Handled by STEP logic below
    }

    // --- CYCLE type (delta==0): ignore sign, just step forward ---
    if (e.delta == 0 && e.steps && e.num_steps > 0) {
        uint8_t current = getCurrentControllerValue(g_midi_ch, cc);
        // Find current step index
        int cur_idx = e.num_steps - 1;
        for (int i = 0; i < e.num_steps; i++) {
            if (current <= e.steps[i]) { cur_idx = i; break; }
        }
        uint8_t next = e.steps[(cur_idx + 1) % e.num_steps];
        handleControlChange(g_midi_ch, cc, next);
        last_param_value = next;
        param_display_timer = 66;
        return;
    }

    // --- STEP type: use threshold array, direction from sign ---
    if (e.steps && e.num_steps > 0) {
        uint8_t current = getCurrentControllerValue(g_midi_ch, cc);
        uint8_t next = current;
        if (sign > 0) {
            bool found = false;
            for (uint8_t i = 0; i < e.num_steps; i++) {
                if (e.steps[i] > current) { next = e.steps[i]; found = true; break; }
            }
            if (!found) next = e.steps[0]; // wrap at top
        } else {
            int found_idx = -1;
            for (int i = e.num_steps - 1; i >= 0; i--) {
                if (e.steps[i] < current) { next = e.steps[i]; found_idx = i; break; }
            }
            if (found_idx == -1) next = e.steps[e.num_steps - 1]; // wrap at bottom
        }
        handleControlChange(g_midi_ch, cc, next);
        
        // Update internal sequencer state for stepped params
        if (cc == SEQ_CC_GATE) sequencer_get()->gate_length = next;
        if (cc == SEQ_CC_SWING) sequencer_get()->swing = next;

        last_param_value = next;
        param_display_timer = 66;
        return;
    }

    // --- CONTINUOUS type ---
    {
        uint8_t current = getCurrentControllerValue(g_midi_ch, cc);
        int val = (int)current + sign * e.delta;
        if (val > 127) val = 127; if (val < 0) val = 0;
        uint8_t next = (uint8_t)val;
        handleControlChange(g_midi_ch, cc, next);
        last_param_value = next;
        param_display_timer = 66;
    }
}

// ---------------------------------------------------------------------------
// Public API: does pad i (0-7) have a parameter in the current group/page?
// ---------------------------------------------------------------------------
bool ui_state_pad_has_param(uint8_t pad) {
    if (pad >= 8) return false;
    int group = (int)current_param_state - (int)PARAM_STATE_OSC;
    if (group < 0 || group > 7) return false;
    const ParamEntry& e = param_table[group][current_param_page][pad];
    return (e.cc != VCC_NONE);
}

bool ui_state_group_has_page2(int group) {
    if (group < 0 || group > 7) return false;
    for (int col = 0; col < 8; col++) {
        if (param_table[group][1][col].cc != VCC_NONE) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// State init / getters
// ---------------------------------------------------------------------------
void ui_state_init(void) {
    current_state = UI_STATE_PIANO; previous_button_states = 0; base_octave_note = 60;
    current_chord_type = CHORD_OFF; current_param_state = PARAM_STATE_SELECT; current_param_page = 0;
    piano_hold_active = false; piano_hold_note = 255;
    sequencer_init();
}

ui_state_t ui_state_get(void) { return current_state; }
uint8_t ui_state_get_base_octave(void) { return base_octave_note; }
chord_type_t ui_state_get_chord_type(void) {
    return (current_state == UI_STATE_SEQ_EDIT) ? (chord_type_t)seq_edit_octave : current_chord_type;
}
param_state_t ui_state_get_param_state(void) { return current_param_state; }
uint8_t ui_state_get_param_page(void) { return current_param_page; }
uint8_t ui_state_get_last_param_value(void) { return last_param_value; }
uint32_t ui_state_get_param_timer(void) { return param_display_timer; }
bool ui_state_piano_is_hold_active(void) { return piano_hold_active; }
uint8_t ui_state_piano_get_hold_note(void) { return piano_hold_note; }

// Flash blink state
static uint8_t flash_blink_pad = 255;
static uint8_t flash_blink_count = 0;
static uint32_t flash_blink_timer = 0;
static bool flash_blink_is_error = false;

uint8_t ui_state_get_flash_blink_pad(void) { return flash_blink_pad; }
uint8_t ui_state_get_flash_blink_count(void) { return flash_blink_count; }
bool ui_state_get_flash_blink_is_error(void) { return flash_blink_is_error; }

bool ui_state_get_flash_blink_state(void) {
    if (flash_blink_timer == 0) return false;
    return (flash_blink_timer % 20) >= 10;
}

void ui_state_set_flash_blink(uint8_t pad, uint8_t count, bool is_error) {
    flash_blink_pad = pad;
    flash_blink_count = count;
    flash_blink_is_error = is_error;
    flash_blink_timer = count * 20; // 20 frames per blink toggle
}

void ui_state_update_timers(void) { 
    if (param_display_timer > 0) param_display_timer--; 
    if (flash_blink_timer > 0) {
        flash_blink_timer--;
        if (flash_blink_timer == 0) {
            flash_blink_pad = 255;
            flash_blink_count = 0;
        } else {
            flash_blink_count = (flash_blink_timer + 19) / 20;
        }
    }
}

// ---------------------------------------------------------------------------
// Main button processing
// ---------------------------------------------------------------------------
void ui_state_process_buttons(uint32_t button_states) {
    uint32_t changed = button_states ^ previous_button_states;
    uint32_t pressed = changed & button_states;
    sequencer_t *seq = sequencer_get();

#if CURRENT_BOARD == BOARD_OMSK
    // OMSK logic (encoder-based, not changed)
#elif CURRENT_BOARD == BOARD_NIZKOTENO
    bool f1_pressed = is_pad_pressed(button_states, BTN_PAD_9);
    bool f2_pressed = is_pad_pressed(button_states, BTN_PAD_10);

    switch (current_state) {
        case UI_STATE_PIANO: {
            bool f1_f2_combo = (f1_pressed && is_pad_just_pressed(pressed, BTN_PAD_10)) || (f2_pressed && is_pad_just_pressed(pressed, BTN_PAD_9));
            if (f1_f2_combo) {
                piano_hold_active = !piano_hold_active;
                if (!piano_hold_active && piano_hold_note != 255) {
                    handleNoteOff(g_midi_ch, piano_hold_note, 0);
                    piano_hold_note = 255;
                }
                break;
            }

            if (!f1_pressed && is_pad_just_pressed(pressed, BTN_PAD_8)) {
                base_octave_note += 12; if (base_octave_note > 108) base_octave_note = 24;
            }
            if (f1_pressed) {
                for (int i = 0; i < 8; i++)
                    if (is_pad_just_pressed(pressed, (logical_button_t)i))
                        current_chord_type = (chord_type_t)i;
            } else {
                const uint8_t scale[7] = {0, 2, 4, 5, 7, 9, 11};
                uint8_t current_note = 255;

                // Detect sharps (combo priority)
                if      (is_pad_pressed(button_states, BTN_PAD_1) && is_pad_pressed(button_states, BTN_PAD_2)) current_note = base_octave_note + 1;
                else if (is_pad_pressed(button_states, BTN_PAD_2) && is_pad_pressed(button_states, BTN_PAD_3)) current_note = base_octave_note + 3;
                else if (is_pad_pressed(button_states, BTN_PAD_4) && is_pad_pressed(button_states, BTN_PAD_5)) current_note = base_octave_note + 6;
                else if (is_pad_pressed(button_states, BTN_PAD_5) && is_pad_pressed(button_states, BTN_PAD_6)) current_note = base_octave_note + 8;
                else if (is_pad_pressed(button_states, BTN_PAD_6) && is_pad_pressed(button_states, BTN_PAD_7)) current_note = base_octave_note + 10;
                else {
                    for (int i = 0; i < 7; i++) {
                        if (is_pad_pressed(button_states, (logical_button_t)i)) {
                            current_note = base_octave_note + scale[i];
                            break;
                        }
                    }
                }

                if (piano_hold_active) {
                    static uint8_t last_detected_note = 255;
                    if (current_note != last_detected_note) {
                        if (current_note != 255) {
                            if (current_note == piano_hold_note) {
                                handleNoteOff(g_midi_ch, piano_hold_note, 0);
                                piano_hold_note = 255;
                            } else {
                                if (piano_hold_note != 255) handleNoteOff(g_midi_ch, piano_hold_note, 0);
                                handleNoteOn(g_midi_ch, current_note, 100);
                                piano_hold_note = current_note;
                            }
                        }
                        last_detected_note = current_note;
                    }
                } else {
                    static uint8_t piano_playing_note = 255;
                    if (current_note != piano_playing_note) {
                        if (piano_playing_note != 255) handleNoteOff(g_midi_ch, piano_playing_note, 0);
                        if (current_note != 255) handleNoteOn(g_midi_ch, current_note, 100);
                        piano_playing_note = current_note;
                    }
                }
            }

            if (!f1_pressed && is_pad_just_pressed(pressed, BTN_PAD_10)) {
                current_state = UI_STATE_PARAMS;
                current_param_state = PARAM_STATE_SELECT;
            }
            break;
        }

        // ----------------------------------------------------------------
        case UI_STATE_SEQ: {
            static bool play_pause_combo_valid = false;
            static logical_button_t pending_save_load_pad = NUM_LOGICAL_BUTTONS;
            static uint32_t save_load_press_time = 0;

            if (is_pad_just_pressed(pressed, BTN_PAD_10)) seq_page_combo_used = false;
            if (is_pad_just_pressed(pressed, BTN_PAD_9)) seq_param_combo_used = false;

            if (f1_pressed && f2_pressed) {
                if (is_pad_just_pressed(pressed, BTN_PAD_9) || is_pad_just_pressed(pressed, BTN_PAD_10)) {
                    play_pause_combo_valid = true;
                    seq_param_combo_used = true;
                    seq_page_combo_used = true;
                }
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        play_pause_combo_valid = false;
                        pending_save_load_pad = (logical_button_t)i;
                        save_load_press_time = time_us_32();
                        seq_param_combo_used = true;
                        seq_page_combo_used = true;
                    }
                }
            } else if (f1_pressed) {
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        editing_step_idx = seq->current_page * 8 + i;
                        editing_note_idx = 0;
                        current_state = UI_STATE_SEQ_EDIT;
                        seq_param_combo_used = true;
                        seq_edit_ignore_done_hold = true;
                        seq_edit_page_combo_used = true;
                        seq_edit_done_combo_used = true;
                    }
                }
            } else if (f2_pressed) {
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        sequencer_set_stop_step(seq->current_page * 8 + i);
                        seq_page_combo_used = true;
                    }
                }
            }

            if (pending_save_load_pad != NUM_LOGICAL_BUTTONS) {
                if (is_pad_just_released(changed, button_states, pending_save_load_pad)) {
                    uint32_t duration = time_us_32() - save_load_press_time;
                    if (duration >= 500000) { // 500ms
                        sequencer_save(pending_save_load_pad);
                        ui_state_set_flash_blink(pending_save_load_pad, 4, false);
                    } else {
                        bool success = sequencer_load(pending_save_load_pad);
                        if (success) {
                            ui_state_set_flash_blink(pending_save_load_pad, 2, false);
                        } else {
                            ui_state_set_flash_blink(pending_save_load_pad, 2, true); // Red error
                        }
                    }
                    pending_save_load_pad = NUM_LOGICAL_BUTTONS;
                } else if (!f1_pressed || !f2_pressed) {
                    pending_save_load_pad = NUM_LOGICAL_BUTTONS;
                }
            }

            if (play_pause_combo_valid && (is_pad_just_released(changed, button_states, BTN_PAD_9) || is_pad_just_released(changed, button_states, BTN_PAD_10))) {
                sequencer_toggle_play();
                play_pause_combo_valid = false;
            }

            if (is_pad_just_released(changed, button_states, BTN_PAD_10) && !seq_page_combo_used && !f1_pressed)
                sequencer_next_page();

            if (!f1_pressed && !f2_pressed) {
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        uint8_t step_idx = seq->current_page * 8 + i;
                        if (seq->steps[step_idx].num_notes == 0) {
                            editing_step_idx = step_idx;
                            editing_note_idx = 0;
                            current_state = UI_STATE_SEQ_EDIT;
                            seq_edit_ignore_done_hold = false;
                            seq_edit_page_combo_used = true;
                            seq_edit_done_combo_used = true;
                        } else {
                            sequencer_toggle_mute(step_idx);
                        }
                    }
                }
            }
            if (is_pad_just_released(changed, button_states, BTN_PAD_9) && !seq_param_combo_used && !f2_pressed) {
                current_state = UI_STATE_PARAMS;
                current_param_state = PARAM_STATE_SELECT;
            }
            break;
        }

        // ----------------------------------------------------------------
        case UI_STATE_SEQ_EDIT: {

            bool done_held = is_pad_pressed(button_states, BTN_PAD_9) && !seq_edit_ignore_done_hold;
            bool page_held = is_pad_pressed(button_states, BTN_PAD_10);
            static uint8_t playing_note = 255;

            if (is_pad_just_pressed(pressed, BTN_PAD_10)) seq_edit_page_combo_used = false;
            if (is_pad_just_pressed(pressed, BTN_PAD_9)) seq_edit_done_combo_used = false;

            if (done_held && !is_pad_just_pressed(pressed, BTN_PAD_9)) {
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        sequencer_set_step_chord(editing_step_idx, (chord_type_t)i);
                        seq_edit_done_combo_used = true;
                    }
                }
            } else if (is_pad_just_released(changed, button_states, BTN_PAD_9)) {
                if (seq_edit_ignore_done_hold) {
                    seq_edit_ignore_done_hold = false;
                } else if (!seq_edit_done_combo_used || current_state != UI_STATE_SEQ_EDIT) {
                    if (playing_note != 255) { handleNoteOff(g_midi_ch, playing_note, 0); playing_note = 255; }
                    if (seq->steps[editing_step_idx].num_notes == 0)
                        seq->steps[editing_step_idx].muted = true;
                    current_state = UI_STATE_SEQ;
                    seq_page_combo_used = true;
                    break;
                }
            }

            if (is_pad_just_released(changed, button_states, BTN_PAD_10) && !seq_edit_page_combo_used) {
                editing_note_idx = (editing_note_idx + 1) % 4;
                if (playing_note != 255) { handleNoteOff(g_midi_ch, playing_note, 0); playing_note = 255; }
            }

            if (page_held) {
                // page held could be used for something else, previously we had chord selection here by mistake
            } else if (!done_held) {
                const uint8_t scale[7] = {0, 2, 4, 5, 7, 9, 11};
                uint8_t note_to_set = 255;

                if      (is_pad_pressed(button_states, BTN_PAD_1) && is_pad_pressed(button_states, BTN_PAD_2)) note_to_set = seq_edit_octave + 1;
                else if (is_pad_pressed(button_states, BTN_PAD_2) && is_pad_pressed(button_states, BTN_PAD_3)) note_to_set = seq_edit_octave + 3;
                else if (is_pad_pressed(button_states, BTN_PAD_4) && is_pad_pressed(button_states, BTN_PAD_5)) note_to_set = seq_edit_octave + 6;
                else if (is_pad_pressed(button_states, BTN_PAD_5) && is_pad_pressed(button_states, BTN_PAD_6)) note_to_set = seq_edit_octave + 8;
                else if (is_pad_pressed(button_states, BTN_PAD_6) && is_pad_pressed(button_states, BTN_PAD_7)) note_to_set = seq_edit_octave + 10;
                else {
                    for (int i = 0; i < 7; i++)
                        if (is_pad_just_pressed(pressed, (logical_button_t)i)) note_to_set = seq_edit_octave + scale[i];
                }

                if (note_to_set != 255) {
                    if (seq->steps[editing_step_idx].num_notes > editing_note_idx &&
                        seq->steps[editing_step_idx].notes[editing_note_idx] == note_to_set) {
                        sequencer_remove_note(editing_step_idx, editing_note_idx);
                        if (playing_note != 255) handleNoteOff(g_midi_ch, playing_note, 0);
                        playing_note = 255;
                    } else {
                        sequencer_set_note(editing_step_idx, editing_note_idx, note_to_set);
                        seq->steps[editing_step_idx].muted = false;
                        if (playing_note != 255) handleNoteOff(g_midi_ch, playing_note, 0);
                        handleNoteOn(g_midi_ch, note_to_set, 100);
                        playing_note = note_to_set;
                    }
                }

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

        // ----------------------------------------------------------------
        case UI_STATE_PARAMS:
            if (current_param_state == PARAM_STATE_SELECT) {
                // Pads 1-8: select group
                for (int i = 0; i < 8; i++) {
                    if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                        current_param_state = (param_state_t)(PARAM_STATE_OSC + i);
                        current_param_page = 0;
                    }
                }
                // Pad 9 → Piano, Pad 10 → Seq
                if (is_pad_just_pressed(pressed, BTN_PAD_9))  current_state = UI_STATE_PIANO;
                if (is_pad_just_pressed(pressed, BTN_PAD_10)) {
                    current_state = UI_STATE_SEQ;
                    seq_page_combo_used = true;
                    if (piano_hold_active) {
                        if (piano_hold_note != 255) handleNoteOff(g_midi_ch, piano_hold_note, 0);
                        piano_hold_active = false;
                        piano_hold_note = 255;
                    }
                }
            } else {
                // Inside a group
                if (is_pad_just_pressed(pressed, BTN_PAD_9))
                    current_param_state = PARAM_STATE_SELECT;
                
                int group = (int)current_param_state - (int)PARAM_STATE_OSC;
                if (is_pad_just_pressed(pressed, BTN_PAD_10)) {
                    if (ui_state_group_has_page2(group)) {
                        current_param_page = (current_param_page + 1) % 2;
                    }
                }

                if (group >= 0 && group <= 7) {
                    // Check for rel=decay combo: EG group, Page 0, (Pad 3+Pad 4 or Pad 7+Pad 8)
                    // Note: Pad indices are 0-based, so Pad 3 is index 2, Pad 4 is index 3, etc.
                    if (group == (PARAM_STATE_EG - PARAM_STATE_OSC) && current_param_page == 0) {
                        bool plus_combo = is_pad_just_pressed(pressed, BTN_PAD_4) && is_pad_pressed(button_states, BTN_PAD_3);
                        bool minus_combo = is_pad_just_pressed(pressed, BTN_PAD_8) && is_pad_pressed(button_states, BTN_PAD_7);
                        if (plus_combo || minus_combo) {
                            uint8_t current = getCurrentControllerValue(g_midi_ch, REL_EQ_DECAY);
                            uint8_t next = (current >= 64) ? 0 : 127;
                            handleControlChange(g_midi_ch, REL_EQ_DECAY, next);
                            last_param_value = next;
                            param_display_timer = 66;
                            // Prevent the single button press from triggering
                            pressed &= ~((1 << BTN_PAD_4) | (1 << BTN_PAD_8));
                        }
                    }

                    for (int i = 0; i < 8; i++) {
                        if (is_pad_just_pressed(pressed, (logical_button_t)i)) {
                            int8_t sign = pad_is_plus(i) ? +1 : -1;
                            const ParamEntry& e = param_table[group][current_param_page][i];
                            // For CYCLE params (~) both + and - row cycle forward
                            adjust_entry(e, sign);
                        }
                    }
                }
            }
            break;

        default: break;
    }
#endif
    previous_button_states = button_states;
}
