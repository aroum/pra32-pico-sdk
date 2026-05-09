#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void i2s_audio_init(uint32_t sample_rate);
void i2s_audio_put_buffer(const int32_t *buffer, size_t length);

#ifdef __cplusplus
}
#endif
