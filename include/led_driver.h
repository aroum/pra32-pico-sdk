#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void led_driver_init(void);
void led_driver_update(void);

#ifdef __cplusplus
}
#endif

#endif
