#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "config.h"
#include "button_driver.h"
#include "debounce.pio.h"
#include "ui_state.h"

static uint32_t current_logical_state = 0;

uint32_t button_driver_get_logical_state(void) {
    return current_logical_state;
}

#if CURRENT_BOARD == BOARD_NIZKOTENO

static PIO debounce_pio = pio0;
static uint sm_debounce = 0;

void button_driver_init(void) {
    uint offset = pio_add_program(debounce_pio, &debounce_program);
    sm_debounce = pio_claim_unused_sm(debounce_pio, true);

    uint pin_base = 1;
    uint pin_count = 10;
    
    float clk_div = (float)CPU_SPEED_KHZ * 1000.0f / (100.0f * 1000.0f);

    for (int i = 0; i < pin_count; i++) {
        gpio_pull_up(pin_base + i);
    }

    debounce_program_init(debounce_pio, sm_debounce, offset, pin_base, pin_count, clk_div);
    ui_state_init();
}

void button_driver_update(void) {
    while (!pio_sm_is_rx_fifo_empty(debounce_pio, sm_debounce)) {
        uint32_t raw_state = pio_sm_get(debounce_pio, sm_debounce);
        uint32_t logical_state = 0;
        
        for (int i = 0; i < NUM_BUTTONS; i++) {
            uint8_t pin = DIRECT_BUTTON_PINS[i];
            bool is_pressed = ((raw_state >> (pin - 1)) & 1) == 0;
            if (is_pressed) {
                logical_state |= (1 << i);
            }
        }
        
        current_logical_state = logical_state;
        ui_state_process_buttons(logical_state);
    }
}

#elif CURRENT_BOARD == BOARD_OMSK

#include "encoder_driver.h"

void button_driver_init(void) {
    for (int i = 0; i < 4; i++) {
        gpio_init(MATRIX_ROWS[i]);
        gpio_set_dir(MATRIX_ROWS[i], GPIO_IN);
        gpio_pull_down(MATRIX_ROWS[i]);

        gpio_init(MATRIX_COLS[i]);
        gpio_set_dir(MATRIX_COLS[i], GPIO_IN);
        gpio_disable_pulls(MATRIX_COLS[i]);
    }

    encoders_init();
    ui_state_init();
}

void button_driver_update(void) {
    uint32_t raw = 0;

    for (int c = 0; c < 4; c++) {
        gpio_set_dir(MATRIX_COLS[c], GPIO_OUT);
        gpio_put(MATRIX_COLS[c], 1);
        sleep_us(5);

        for (int r = 0; r < 4; r++) {
            if (gpio_get(MATRIX_ROWS[r])) {
                raw |= (1u << (r * 4 + c));
            }
        }
        gpio_set_dir(MATRIX_COLS[c], GPIO_IN);
        gpio_disable_pulls(MATRIX_COLS[c]);
    }

    static uint32_t history[2] = {0};
    uint32_t stable = ~(raw ^ history[0]) & ~(raw ^ history[1]);
    uint32_t changed = raw ^ current_logical_state;
    uint32_t flip = changed & stable;
    current_logical_state ^= flip;

    history[1] = history[0];
    history[0] = raw;

    ui_state_process_buttons(current_logical_state);
}

#else
// Placeholder for other boards
void button_driver_init(void) { ui_state_init(); }
void button_driver_update(void) {}
#endif
