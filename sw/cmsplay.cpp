/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <math.h>

#if PICO_ON_DEVICE

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#endif

#include "pico/stdlib.h"

#include "pico/audio_i2s.h"

// #include "SAASound.h"
// extern LPCSAASOUND saa0, saa1;
#include "saa1099/saa1099.h"
extern saa1099_device *saa0, *saa1;

#if PICO_ON_DEVICE
#include "pico/binary_info.h"
bi_decl(bi_3pins_with_names(PICO_AUDIO_I2S_DATA_PIN, "I2S DIN", PICO_AUDIO_I2S_CLOCK_PIN_BASE, "I2S BCK", PICO_AUDIO_I2S_CLOCK_PIN_BASE+1, "I2S LRCK"));
#endif

#define SAMPLES_PER_BUFFER 256

struct audio_buffer_pool *init_audio() {

    static audio_format_t audio_format = {
            .sample_freq = 44100,
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .channel_count = 2,
    };

    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 4
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 2,
                                                                      SAMPLES_PER_BUFFER); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;
    struct audio_i2s_config config = {
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
            .dma_channel = 6,
            .pio_sm = 1,
    };

    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    //ok = audio_i2s_connect(producer_pool);
    ok = audio_i2s_connect_extra(producer_pool, false, 0, 0, NULL);
    assert(ok);
    audio_i2s_set_enabled(true);
    return producer_pool;
}

void play_cms() {
    puts("starting core 1 CMS");
    saa0->device_start();
    saa1->device_start();
    uint32_t start, end;
    struct audio_buffer_pool *ap = init_audio();
    int16_t buf0[SAMPLES_PER_BUFFER * 2];
    int16_t buf1[SAMPLES_PER_BUFFER * 2];
    // int32_t play_buf[SAMPLES_PER_BUFFER *2];
    for (;;) {
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        // putchar('/');
    
        // uint32_t cms_audio_begin = time_us_32();
        saa0->sound_stream_update(buf0, SAMPLES_PER_BUFFER);
        saa1->sound_stream_update(buf1, SAMPLES_PER_BUFFER);
        for(uint32_t i = 0; i < SAMPLES_PER_BUFFER; ++i) {
            samples[i << 1] = ((int32_t)buf0[i << 1] + (int32_t)buf1[i << 1]) >> 1;
            samples[(i << 1) + 1] = ((int32_t)buf0[(i << 1) + 1] + (int32_t)buf1[(i << 1) + 1]) >> 1;
        }
        // uint32_t cms_audio_elapsed = time_us_32() - cms_audio_begin;
        // printf("%u ", cms_audio_elapsed);
        buffer->sample_count = buffer->max_sample_count;

        give_audio_buffer(ap, buffer);
    }
}
