#ifndef I2S_H
#define I2S_H

#include <driver/i2s_std.h>
#include <esp_err.h>

#define I2S_INMP441_SCK (gpio_num_t)39 // FIX
#define I2S_INMP441_WS (gpio_num_t)40  // FIX
#define I2S_INMP441_SD (gpio_num_t)9   // FIX

#define I2S_SAMPLE_RATE (8000U)
#define I2S_SAMPLE_BYTES (4U)
#define I2S_SAMPLE_COUNT (16384U)

extern i2s_chan_handle_t s_rx_handle;

void mic_init();
void mic_deinit();
size_t mic_read(int16_t* samples, size_t max_samples);

#endif // I2S_H
