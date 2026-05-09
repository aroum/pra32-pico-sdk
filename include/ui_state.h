#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "ui_types.h"

#define NUM_ENCODERS 4

extern uint8_t editing_step_idx;
extern uint8_t editing_note_idx;
extern uint8_t seq_edit_octave;

void ui_state_init(void);
void ui_state_process_buttons(uint32_t button_states);
ui_state_t ui_state_get(void);
uint8_t ui_state_get_base_octave(void);
chord_type_t ui_state_get_chord_type(void);
param_state_t ui_state_get_param_state(void);
uint8_t ui_state_get_param_page(void);
uint8_t ui_state_get_last_param_value(void);
uint32_t ui_state_get_param_timer(void);
void ui_state_update_timers(void);
