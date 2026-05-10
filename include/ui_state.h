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
bool ui_state_pad_has_param(uint8_t pad); // 0-7: true if pad has assignment in current group/page
bool ui_state_group_has_page2(int group); // true if group has parameters on page 2

uint8_t ui_state_get_flash_blink_pad(void);
uint8_t ui_state_get_flash_blink_count(void);
bool ui_state_get_flash_blink_is_error(void);
bool ui_state_get_flash_blink_state(void);
void ui_state_set_flash_blink(uint8_t pad, uint8_t count, bool is_error);

bool ui_state_seq_edit_is_done_held(uint32_t button_states);
bool ui_state_piano_is_hold_active(void);
uint8_t ui_state_piano_get_hold_note(void);
