#include "ui_oled.h"
#include "config.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "u8g2.h"
#include <stdio.h>
#include <string.h>

#if CFG_ENABLE_OLED || USE_CONTROL_PANEL_OLED_DISPLAY

static u8g2_t g_u8g2;
static int g_oled_ready = 0;

static uint8_t u8x8_byte_pico_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static uint8_t buffer[1024];
    static uint16_t buf_idx;
    switch (msg) {
        case U8X8_MSG_BYTE_SEND:
            if (buf_idx + arg_int < sizeof(buffer)) {
                memcpy(buffer + buf_idx, arg_ptr, arg_int);
                buf_idx += arg_int;
            }
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            buf_idx = 0;
            break;
        case U8X8_MSG_BYTE_END_TRANSFER: {
            uint8_t addr = u8x8_GetI2CAddress(u8x8) >> 1;
            i2c_write_timeout_us(OLED_I2C_INSTANCE, addr, buffer, buf_idx, false, 50000);
            break;
        }
    }
    return 1;
}

static uint8_t u8x8_gpio_and_delay_pico(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_DELAY_MILLI: sleep_ms(arg_int); break;
        case U8X8_MSG_DELAY_10MICRO: sleep_us(arg_int * 10); break;
        case U8X8_MSG_DELAY_I2C: sleep_us(2); break;
    }
    return 1;
}

void ui_oled_init(void) {
    i2c_init(OLED_I2C_INSTANCE, 400 * 1000);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SCL_PIN);
    gpio_pull_up(OLED_SDA_PIN);

    // TODO: Switch based on OLED_DRIVER if needed
    u8g2_Setup_ssd1312_i2c_128x64_noname_f(&g_u8g2, U8G2_R0, u8x8_byte_pico_hw_i2c, u8x8_gpio_and_delay_pico);
    u8g2_InitDisplay(&g_u8g2);
    u8g2_SetPowerSave(&g_u8g2, 0);
    u8x8_SetI2CAddress(&g_u8g2.u8x8, OLED_I2C_ADDRESS << 1);

    u8g2_ClearBuffer(&g_u8g2);
    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB14_tr);
    u8g2_DrawStr(&g_u8g2, 0, 24, "NIZKOTENO");
    u8g2_SetFont(&g_u8g2, u8g2_font_6x12_tr);
    u8g2_DrawStr(&g_u8g2, 0, 44, (CURRENT_BOARD == BOARD_OMSK) ? "OMSK EDITION" : "PRA32 EDITION");
    u8g2_SendBuffer(&g_u8g2);

    g_oled_ready = 1;
}

void ui_oled_draw(const OledPage *page) {}
void ui_oled_set_power(bool on) { if (g_oled_ready) u8g2_SetPowerSave(&g_u8g2, on ? 0 : 1); }
void ui_oled_set_brightness(uint8_t percentage) {
    if (g_oled_ready) {
        uint8_t contrast = (uint8_t)((percentage * 255) / 100);
        u8g2_SetContrast(&g_u8g2, contrast);
    }
}

#else
void ui_oled_init(void) {}
void ui_oled_draw(const OledPage *page) {}
void ui_oled_set_power(bool on) {}
void ui_oled_set_brightness(uint8_t percentage) {}
#endif
