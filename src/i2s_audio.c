#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/clocks.h"
#include "audio_i2s.pio.h"

#define I2S_DATA_PIN 9
#define I2S_BCLK_PIN 10
#define I2S_LRCLK_PIN 11

static PIO i2s_pio = pio0;
static uint i2s_sm = 0;
static int i2s_dma_chan;

void i2s_audio_init(uint32_t sample_rate) {
    uint offset = pio_add_program(i2s_pio, &audio_i2s_program);
    audio_i2s_program_init(i2s_pio, i2s_sm, offset, I2S_DATA_PIN, I2S_BCLK_PIN);

    float system_clock = clock_get_hz(clk_sys);
    // BCLK is 32 bits per sample (16 L + 16 R)
    // One cycle of PIO program is 2 BCLKs (one for left, one for right? No, standard I2S is bit-by-bit)
    // Actually, the standard PIO I2S program usually shifts out 32 bits per frame.
    // Let's assume a standard 32-bit frame (16 bits per channel).
    float divider = system_clock / (sample_rate * 32.0f * 2.0f); // 2 clocks per bit in some implementations
    // This depends on the .pio program. I'll provide a standard one.
    pio_sm_set_clkdiv(i2s_pio, i2s_sm, divider);

    i2s_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(i2s_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(i2s_pio, i2s_sm, true));

    dma_channel_configure(
        i2s_dma_chan,
        &c,
        &i2s_pio->txf[i2s_sm],
        NULL,
        0,
        false
    );
}

void i2s_audio_put_buffer(const int32_t *buffer, size_t length) {
    dma_channel_wait_for_finish_blocking(i2s_dma_chan);
    dma_channel_transfer_from_buffer_now(i2s_dma_chan, buffer, length);
}
