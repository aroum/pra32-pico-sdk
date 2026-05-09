#include "encoder_driver.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "config.h"

#if CURRENT_BOARD == BOARD_OMSK

typedef struct {
    uint pin_a;
    uint pin_b;
    volatile int value;
    uint32_t last_us;
} encoder_t;

static encoder_t encoders[4];

static void gpio_callback(uint gpio, uint32_t events) {
    (void)events;
    for (int i = 0; i < 4; i++) {
        if (gpio == encoders[i].pin_a) {
            uint32_t now = time_us_32();
            uint32_t dt = now - encoders[i].last_us;
            // Basic debounce
            if (dt < 2000) {
                return;
            }
            encoders[i].last_us = now;
            if (gpio_get(encoders[i].pin_b)) {
                encoders[i].value--;
            } else {
                encoders[i].value++;
            }
            break;
        }
    }
}

void encoders_init(void) {
    for (int i = 0; i < 4; i++) {
        encoders[i].pin_a = OMSK_ENCODER_PINS[i][0];
        encoders[i].pin_b = OMSK_ENCODER_PINS[i][1];
        encoders[i].value = 0;
        encoders[i].last_us = 0;

        gpio_init(encoders[i].pin_a);
        gpio_set_dir(encoders[i].pin_a, GPIO_IN);
        gpio_pull_up(encoders[i].pin_a);
        
        gpio_init(encoders[i].pin_b);
        gpio_set_dir(encoders[i].pin_b, GPIO_IN);
        gpio_pull_up(encoders[i].pin_b);
        
        gpio_set_irq_enabled_with_callback(encoders[i].pin_a, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    }
}

int encoders_get_delta(int index) {
    if (index < 0 || index >= 4) return 0;
    
    // We only want to return a delta if it's significant (e.g. 2 steps per click or 4 steps per click)
    // Most encoders are 2 or 4 steps per click.
    int val = encoders[index].value;
    if (val == 0) return 0;
    
    // Reset after reading
    encoders[index].value = 0;
    return val;
}

#endif
