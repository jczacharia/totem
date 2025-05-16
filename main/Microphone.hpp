#pragma once

#include <cmath>
#include <vector>
#include <array>
#include <mutex>
#include <driver/i2s_std.h>

#include "util/Fft.hpp"
#include "util/ThreadManager.hpp"

/**
 * @brief Class for controlling the INMP441 I2S microphone on ESP32
 */
class Microphone final
{
public:
    static constexpr size_t SAMPLE_RATE = 22050;
    static constexpr size_t BUFFER_SIZE = 512;
    static constexpr size_t MAX_FREQ_BINS = 64; // Maximum number of frequency bins we'll support

    Microphone() = delete;

private:
    static constexpr auto TAG = "Microphone";

    // Microphone data acquisition 
    static std::mutex read_mic_mutex_;
    static std::array<int32_t, BUFFER_SIZE> buffer_;
    static i2s_chan_handle_t rx_chan_;

    // Thread management
    static ThreadManager* processing_thread_;
    static std::atomic<bool> thread_initialized_;

    // Double-buffering system for spectrum data
    static std::mutex spectrum_mutex_;
    static std::array<float, MAX_FREQ_BINS> spectrum_buffer_[2]; // Double buffer
    static std::atomic<uint8_t> active_buffer_; // Which buffer is currently being read by clients
    static std::atomic<uint32_t> update_count_; // Counter to track updates for clients
    static std::atomic<uint32_t> processing_time_us_; // Time taken for processing

    struct Pin
    {
        static constexpr gpio_num_t WS = GPIO_NUM_15;
        static constexpr gpio_num_t SCK = GPIO_NUM_14;
        static constexpr gpio_num_t SD = GPIO_NUM_32;
    };

    // Thread function for FFT processing
    static void processingThreadFunc(const std::atomic<bool>& running)
    {
        std::vector<std::complex<float>> fft_input(BUFFER_SIZE);
        std::array<int32_t, BUFFER_SIZE> local_buffer{};
        std::array<float, MAX_FREQ_BINS> local_spectrum{};

        ESP_LOGI(TAG, "FFT processing thread started on core %d", xPortGetCoreID());

        uint32_t last_process_time = 0;

        while (running)
        {
            // Don't process too frequently to avoid CPU overload
            if (constexpr uint32_t min_interval_ms = 30;
                esp_timer_get_time() / 1000 - last_process_time < min_interval_ms)
            {
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }

            // Read microphone data with timeout
            size_t bytes_read = 0;
            {
                std::lock_guard lock(read_mic_mutex_);
                const esp_err_t err = i2s_channel_read(rx_chan_, buffer_.data(),
                                                       BUFFER_SIZE * sizeof(int32_t),
                                                       &bytes_read, 100 / portTICK_PERIOD_MS); // 100ms timeout

                if (err != ESP_OK)
                {
                    // Just skip this cycle if there's an error
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }

                // Copy to local buffer for processing
                std::memcpy(local_buffer.data(), buffer_.data(), sizeof(local_buffer));
            }

            if (bytes_read < BUFFER_SIZE * sizeof(int32_t))
            {
                // Pad with zeros if we didn't get a full buffer
                for (size_t i = bytes_read / sizeof(int32_t); i < BUFFER_SIZE; ++i)
                {
                    local_buffer[i] = 0;
                }
            }

            const auto start_time = esp_timer_get_time();

            // Prepare FFT input with window function
            for (size_t i = 0; i < BUFFER_SIZE; ++i)
            {
                const float cos_res = cosf(2.0f * M_PI * static_cast<float>(i) / static_cast<float>(BUFFER_SIZE - 1));
                const float window_val = 0.5f * (1.0f - cos_res);
                const float sample_float = static_cast<float>(local_buffer[i] >> 16) / 32768.0f;
                fft_input[i] = std::complex(sample_float * window_val, 0.0f);
            }

            // Perform FFT
            util::fft(fft_input);

            // Calculate magnitude spectrum
            for (size_t i = 0; i < MAX_FREQ_BINS; ++i)
            {
                const float real_part = fft_input[i].real();
                const float imag_part = fft_input[i].imag();
                local_spectrum[i] = std::sqrtf(real_part * real_part + imag_part * imag_part);
            }

            // Track processing time
            processing_time_us_.store(esp_timer_get_time() - start_time);
            last_process_time = esp_timer_get_time() / 1000;

            // Store results in inactive buffer
            const uint8_t write_buffer = 1 - active_buffer_.load();
            {
                std::lock_guard lock(spectrum_mutex_);
                std::memcpy(spectrum_buffer_[write_buffer].data(), local_spectrum.data(),
                            sizeof(float) * MAX_FREQ_BINS);
            }

            // Switch active buffer
            active_buffer_.store(write_buffer);
            update_count_.fetch_add(1);

            // Yield to other tasks
            vTaskDelay(pdMS_TO_TICKS(1));
        }

        ESP_LOGI(TAG, "FFT processing thread ended");
    }

public:
    static esp_err_t start()
    {
        ESP_LOGI(TAG, "Starting Microphone and FFT processing...");

        // Initialize spectrum buffers
        for (auto& buffer : spectrum_buffer_)
        {
            buffer.fill(0.0f);
        }

        active_buffer_.store(0);
        update_count_.store(0);
        processing_time_us_.store(0);
        thread_initialized_.store(false);

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

        esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &rx_chan_);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create I2S RX channel: %s", esp_err_to_name(err));
            return err;
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

        err = i2s_channel_init_std_mode(rx_chan_, &std_cfg);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize I2S RX channel in STD mode: %s", esp_err_to_name(err));
            i2s_del_channel(rx_chan_);
            return err;
        }

        err = i2s_channel_enable(rx_chan_);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to enable I2S RX channel: %s", esp_err_to_name(err));
            i2s_channel_disable(rx_chan_);
            i2s_del_channel(rx_chan_);
            return err;
        }

        // Start the FFT processing thread on core 0
        static constexpr int FFT_THREAD_CORE = 0;
        static constexpr size_t FFT_THREAD_STACK_SIZE = 8192;
        static constexpr int FFT_THREAD_PRIORITY = 5; // Lower number = higher priority

        processing_thread_ = new ThreadManager(
            "MicFFT", FFT_THREAD_CORE,
            FFT_THREAD_STACK_SIZE,
            FFT_THREAD_PRIORITY);

        processing_thread_->start(processingThreadFunc);
        thread_initialized_.store(true);

        ESP_LOGI(TAG, "Microphone and FFT processing running");
        return ESP_OK;
    }

    static void stop()
    {
        ESP_LOGI(TAG, "Destroying Microphone and FFT processing...");

        // Stop processing thread first
        if (thread_initialized_.load())
        {
            delete processing_thread_;
            processing_thread_ = nullptr;
            thread_initialized_.store(false);
        }

        // Then shutdown the I2S channel
        if (rx_chan_)
        {
            i2s_channel_disable(rx_chan_);
            i2s_del_channel(rx_chan_);
        }

        ESP_LOGI(TAG, "Microphone and FFT processing destroyed");
    }

    // New getSpectrum implementation - just copies from pre-processed buffer
    template <std::size_t TFreqBins>
    static void getSpectrum(std::array<float, TFreqBins>& spectrum)
    {
        // Ensure we don't read more bins than we have data for
        static_assert(TFreqBins <= MAX_FREQ_BINS, "Requested frequency bins exceed maximum supported");

        // Just copy data from the current active buffer - very fast operation
        {
            const uint8_t current_buffer = active_buffer_.load();
            std::lock_guard lock(spectrum_mutex_);
            std::memcpy(spectrum.data(), spectrum_buffer_[current_buffer].data(), sizeof(float) * TFreqBins);
        }
    }

    // Get the time taken for FFT processing (for diagnostics)
    static uint32_t getProcessingTimeUs()
    {
        return processing_time_us_.load();
    }

    // Get update count for clients to detect new data
    static uint32_t getUpdateCount()
    {
        return update_count_.load();
    }
};

// Initialize static members
std::mutex Microphone::read_mic_mutex_;
std::mutex Microphone::spectrum_mutex_;
std::array<int32_t, Microphone::BUFFER_SIZE> Microphone::buffer_;
i2s_chan_handle_t Microphone::rx_chan_;
ThreadManager* Microphone::processing_thread_ = nullptr;
std::atomic<bool> Microphone::thread_initialized_(false);
std::array<float, Microphone::MAX_FREQ_BINS> Microphone::spectrum_buffer_[2];
std::atomic<uint8_t> Microphone::active_buffer_(0);
std::atomic<uint32_t> Microphone::update_count_(0);
std::atomic<uint32_t> Microphone::processing_time_us_(0);
