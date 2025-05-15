#pragma once

#include <cmath>
#include <vector>
#include <driver/i2s_std.h>

#include "util/Singleton.hpp"

#include "matrix/LedMatrix.hpp"

/**
 * @brief Class for controlling the INMP441 I2S microphone on ESP32
 */
class Microphone final : public util::Singleton<Microphone>
{
public:
    static constexpr size_t SAMPLE_RATE = 22050;
    static constexpr size_t BUFFER_SIZE = 512;

private:
    static constexpr auto TAG = "Microphone";
    friend class util::Singleton<Microphone>;

    std::array<int32_t, BUFFER_SIZE> buffer_{};

    struct Pin
    {
        static constexpr gpio_num_t WS = GPIO_NUM_15;
        static constexpr gpio_num_t SCK = GPIO_NUM_14;
        static constexpr gpio_num_t SD = GPIO_NUM_32;
    };

    i2s_chan_handle_t _rx_chan;

    Microphone()
    {
        ESP_LOGI(TAG, "Initializing I2S microphone with STD driver");

        constexpr i2s_chan_config_t chan_cfg = {
            .id = I2S_NUM_1,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = 8,
            .dma_frame_num = BUFFER_SIZE / 2,
            .auto_clear = true,
            .auto_clear_before_cb = true,
            .allow_pd = false,
            .intr_priority = 0,
        };

        esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &_rx_chan);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create I2S RX channel: %s", esp_err_to_name(err));
            return;
        }

        constexpr i2s_std_config_t std_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = Pin::SCK,
                .ws = Pin::WS,
                .dout = I2S_GPIO_UNUSED,
                .din = Pin::SD,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
            },
        };

        err = i2s_channel_init_std_mode(_rx_chan, &std_cfg);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize I2S RX channel in STD mode: %s", esp_err_to_name(err));
            i2s_del_channel(_rx_chan);
            return;
        }

        err = i2s_channel_enable(_rx_chan);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to enable I2S RX channel: %s", esp_err_to_name(err));
            i2s_channel_disable(_rx_chan);
            i2s_del_channel(_rx_chan);
            return;
        }

        ESP_LOGI(TAG, "I2S RX channel initialized and enabled successfully");
    }

    ~Microphone() override
    {
        ESP_LOGI(TAG, "Shutting down microphone");
        if (_rx_chan)
        {
            i2s_channel_disable(_rx_chan);
            i2s_del_channel(_rx_chan);
        }
    }

public:
    template <std::size_t TFreqBins>
    void getSpectrum(std::array<float, TFreqBins>& spectrum)
    {
        size_t bytes_read = 0;
        const esp_err_t err = i2s_channel_read(_rx_chan, buffer_.data(),
                                               BUFFER_SIZE * sizeof(int32_t),
                                               &bytes_read, portMAX_DELAY);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read from microphone: %s", esp_err_to_name(err));
            return;
        }

        if (bytes_read < BUFFER_SIZE * sizeof(int32_t))
        {
            ESP_LOGW(TAG, "Read fewer bytes (%zu) than expected (%zu). Padding with zeros.",
                     bytes_read, BUFFER_SIZE * sizeof(int32_t));
            for (size_t i = bytes_read / sizeof(int32_t); i < BUFFER_SIZE; ++i)
            {
                buffer_[i] = 0;
            }
        }

        std::vector<std::complex<float>> fft_input(BUFFER_SIZE);

        for (size_t i = 0; i < BUFFER_SIZE; ++i)
        {
            const float cos_res = cosf(2.0f * M_PI * static_cast<float>(i) / static_cast<float>(BUFFER_SIZE - 1));
            const float window_val = 0.5f * (1.0f - cos_res);
            const float sample_float = static_cast<float>(buffer_[i] >> 16) / 32768.0f;
            fft_input[i] = std::complex(sample_float * window_val, 0.0f);
        }

        util::fft(fft_input);

        for (int i = 0; i < TFreqBins; ++i)
        {
            if (i < BUFFER_SIZE / 2)
            {
                const float real_part = fft_input[i].real();
                const float imag_part = fft_input[i].imag();
                spectrum[i] = std::sqrtf(real_part * real_part + imag_part * imag_part);
            }
            else
            {
                spectrum[i] = 0.0f;
            }
        }
    }
};
