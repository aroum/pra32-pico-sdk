#pragma once

#include <stdint.h>

typedef enum {
    UI_STATE_PIANO = 0,
    UI_STATE_PARAMS,
    UI_STATE_SEQ,
    UI_STATE_SEQ_EDIT
} ui_state_t;

typedef enum {
    PARAM_STATE_SELECT = 0,
    PARAM_STATE_OSC,
    PARAM_STATE_MIX,
    PARAM_STATE_FILT,
    PARAM_STATE_EG,
    PARAM_STATE_LFO,
    PARAM_STATE_AMP,
    PARAM_STATE_FX,
    PARAM_STATE_MISC
} param_state_t;

typedef enum {
    CHORD_MAJOR = 0,
    CHORD_MINOR,
    CHORD_POWER,
    CHORD_MAJ7,
    CHORD_7,
    CHORD_M7,
    CHORD_DIM,
    CHORD_OFF,
    NUM_CHORD_TYPES
} chord_type_t;

typedef enum {
    BTN_PAD_1 = 0, // pad_1
    BTN_PAD_2,     // pad_2
    BTN_PAD_3,     // pad_3
    BTN_PAD_4,     // pad_4
    BTN_PAD_5,     // pad_5
    BTN_PAD_6,     // pad_6
    BTN_PAD_7,     // pad_7
    BTN_PAD_8,     // pad_8
    BTN_PAD_9,     // pad_9
    BTN_PAD_10,    // pad_10
    BTN_PAD_11,    // pad_11
    BTN_PAD_12,    // pad_12
    BTN_PAD_13,    // pad_13
    BTN_PAD_14,    // pad_14
    BTN_PAD_15,    // pad_15
    BTN_PAD_16,    // pad_16
    NUM_LOGICAL_BUTTONS
} logical_button_t;
