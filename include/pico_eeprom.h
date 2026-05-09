#pragma once
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/multicore.h"
#include <string.h>

#define FLASH_TARGET_OFFSET (1024 * 1024) // 1MB offset, adjust as needed

class PicoEEPROM {
    uint8_t buffer[2048];
    bool dirty;

public:
    PicoEEPROM() : dirty(false) {
        memset(buffer, 0xFF, sizeof(buffer));
    }

    void begin(size_t size) {
        // Load from flash
        const uint8_t* flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);
        memcpy(buffer, flash_target_contents, sizeof(buffer));
    }

    uint8_t read(int address) {
        if (address < 0 || address >= (int)sizeof(buffer)) return 0xFF;
        return buffer[address];
    }

    void write(int address, uint8_t value) {
        if (address < 0 || address >= (int)sizeof(buffer)) return;
        if (buffer[address] != value) {
            buffer[address] = value;
            dirty = true;
        }
    }

    bool commit() {
        if (!dirty) return true;
        
        multicore_lockout_start_blocking();
        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_TARGET_OFFSET, buffer, sizeof(buffer));
        restore_interrupts(ints);
        multicore_lockout_end_blocking();
        
        dirty = false;
        return true;
    }
};

extern PicoEEPROM EEPROM;
