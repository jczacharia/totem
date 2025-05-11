#pragma once

#include "lib/Singleton.hpp"

#include <driver/i2s_std.h>

static constexpr auto samples = 512;
static int32_t buffer32[samples] = {};

class Microphone final : public Singleton<Microphone>
{
    static constexpr auto TAG = "Microphone";
    friend class Singleton<Microphone>;

    static constexpr i2s_port_t I2S_PORT = I2S_NUM_1;
    static constexpr uint32_t I2S_SAMPLE_RATE = 16000;
    static constexpr int I2S_BUFFER_SIZE = 2048;
    static constexpr i2s_data_bit_width_t I2S_DATA_BITS = I2S_DATA_BIT_WIDTH_24BIT;
    static constexpr i2s_slot_bit_width_t I2S_SLOT_BITS = I2S_SLOT_BIT_WIDTH_32BIT;
    static constexpr i2s_std_slot_mask_t I2S_MIC_SLOT_MASK = I2S_STD_SLOT_LEFT;

    i2s_chan_handle_t _rx_chan;

    // Automagic gain control: 0 - none, 1 - normal, 2 - vivid, 3 - lazy (config value)
    static constexpr uint8_t soundAgc = 2;
    float micDataReal = 0.0f;

    struct Pin
    {
        static constexpr gpio_num_t WS = GPIO_NUM_15;
        static constexpr gpio_num_t SCK = GPIO_NUM_14;
        static constexpr gpio_num_t SD = GPIO_NUM_32;
    };

    std::thread _fft_thread;
    std::atomic<bool> _fft_thread_running{true};

    // void provisionFftThread()
    // {
    //     ESP_LOGI(TAG, "Provisioning FFT thread");
    //     auto cfg = esp_pthread_get_default_config();
    //     cfg.thread_name = "microphone_fft_thread";
    //     cfg.pin_to_core = 1;
    //     cfg.stack_size = 4096;
    //     cfg.prio = configMAX_PRIORITIES - 3;
    //     esp_pthread_set_cfg(&cfg);
    //     const auto sleep_time = std::chrono::milliseconds(1);
    //     _fft_thread = std::thread([this, sleep_time]
    //     {
    //         while (_fft_thread_running)
    //         {
    //             std::this_thread::sleep_for(sleep_time);
    //
    //             uint32_t audio_time = Util::millis();
    //             static unsigned long lastUMRun = Util::millis();
    //
    //             const unsigned long t_now = Util::millis();
    //             int userLoopDelay = static_cast<int>(t_now - lastUMRun);
    //
    //             if (lastUMRun == 0)
    //             {
    //                 userLoopDelay = 0; // startup - don't have valid data from last run.
    //             }
    //
    //             // run filters, and repeat in case of loop delays (hick-up compensation)
    //             if (userLoopDelay < 2)
    //             {
    //                 userLoopDelay = 0; // minor glitch, no problem
    //             }
    //
    //             if (userLoopDelay > 200)
    //             {
    //                 userLoopDelay = 200; // limit number of filter re-runs
    //             }
    //
    //             do
    //             {
    //                 // getSample(); // run microphone sampling filters
    //                 // agcAvg(t_now - userLoopDelay); // Calculated the PI adjusted value as sampleAvg
    //                 userLoopDelay -= 2; // advance "simulated time" by 2ms
    //             }
    //             while (userLoopDelay > 0);
    //         }
    //     });
    //     _fft_thread.detach();
    // }

    Microphone()
    {
        ESP_LOGI(TAG, "Initializing I2S microphone with STD driver.");

        // 1. Configure the I2S channel
        constexpr i2s_chan_config_t chan_cfg = {
            .id = I2S_PORT,
            .role = I2S_ROLE_MASTER, // ESP32 is I2S Master
            .dma_desc_num = 8, // Number of DMA descriptors
            .dma_frame_num = 128, // DMA frame size (samples per frame), 128*4 = 512 bytes
            // I2S_READ_BUFFER_SIZE should be a multiple of (dma_frame_num * bytes_per_sample)
            .auto_clear = false, // TX specific, not used for RX
            .auto_clear_before_cb = false, // Typically for TX, safe default for RX
            .allow_pd = false, // Power down disabled by default
            .intr_priority = 0, // Default interrupt priority (or ESP_INTR_FLAG_LEVEL1)
        };

        esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &_rx_chan); // Allocate RX channel
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create I2S RX channel: %s", esp_err_to_name(err));
            return;
        }

        // 2. Configure the I2S standard mode
        i2s_std_config_t std_cfg = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE), // Default clock config for sample rate
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BITS, I2S_SLOT_MODE_MONO), // Philips/I2S format
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = Pin::SCK,
                .ws = Pin::WS,
                .dout = I2S_GPIO_UNUSED,
                .din = Pin::SD,
                .invert_flags = {
                    // Default inversion flags (no inversion)
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false,
                },
            },
        };

        // Specific slot configuration for INMP441
        std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BITS; // Explicitly set slot width (INMP441 uses 32-bit frame)
        std_cfg.slot_cfg.slot_mask = I2S_MIC_SLOT_MASK; // Select left or right channel based on L/R pin

        // Initialize the channel with the standard mode configuration
        err = i2s_channel_init_std_mode(_rx_chan, &std_cfg);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize I2S RX channel in STD mode: %s", esp_err_to_name(err));
            i2s_del_channel(_rx_chan);
            return;
        }

        // 3. Enable the I2S channel
        err = i2s_channel_enable(_rx_chan);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to enable I2S RX channel: %s", esp_err_to_name(err));
            i2s_channel_disable(_rx_chan);
            i2s_del_channel(_rx_chan);
            return;
        }

        ESP_LOGI(TAG, "I2S RX channel initialized and enabled successfully.");

        size_t bytesRead = 0;


        err = i2s_channel_read(_rx_chan, &buffer32, sizeof(buffer32), &bytesRead, portMAX_DELAY);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read RX channel: %s", esp_err_to_name(err));
            i2s_channel_disable(_rx_chan);
            i2s_del_channel(_rx_chan);
            return;
        }

        if (const int samplesRead = bytesRead / 4; samplesRead == samples)
        {
            float mean = 0.0;
            ESP_LOGI(TAG, "Read the expected %d samples", samples);

            for (int i = 0; i < samplesRead; i++)
            {
                mean += abs(buffer32[i] >> 16);
            }

            if (mean != 0.0)
            {
                ESP_LOGI(TAG, "...and they weren't all zeros! Working!");
            }
            else
            {
                ESP_LOGE(TAG, "...and they were all zeros.");
                ESP_LOGE(TAG, "Check the mic pin definitions and L/R pin and "
                         "potentially switch L/R to VCC from GND or vice-versa.");
            }
        }
        else
        {
            ESP_LOGE(TAG, "No samples read!");
        }
    }

    ~Microphone() override
    {
        ESP_LOGI(TAG, "Microphone destructor called");

        _fft_thread_running = false;
        if (_fft_thread.joinable()) _fft_thread.join();
    }
};
