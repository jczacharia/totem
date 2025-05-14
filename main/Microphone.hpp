#pragma once

#include "util/Singleton.hpp"
#include <driver/i2s_std.h>
#include <vector>
#include <cmath>

/**
 * @brief Class for controlling the INMP441 I2S microphone on ESP32
 */
class Microphone final : public util::Singleton<Microphone>
{
public:
    static constexpr size_t DEFAULT_BUFFER_SIZE = 512;
    static constexpr size_t SPECTRUM_SIZE = LedMatrix::MATRIX_WIDTH;
    using SampleBuffer = std::vector<int32_t>;

    using SpectrumBuffer = std::array<float, LedMatrix::MATRIX_WIDTH>;

    [[nodiscard]] const SampleBuffer& getBuffer() const
    {
        return buffer_;
    }

    /**
     * @brief Get a reference to the most recent audio samples
     * @param num_samples Number of samples to read (default: DEFAULT_BUFFER_SIZE)
     * @return Const reference to the internal buffer containing audio samples
     */
    size_t read(const size_t num_samples = DEFAULT_BUFFER_SIZE)
    {
        if (num_samples > buffer_.size())
        {
            buffer_.resize(num_samples);
        }

        size_t bytes_read = 0;
        const esp_err_t err = i2s_channel_read(_rx_chan, buffer_.data(),
                                               num_samples * sizeof(int32_t),
                                               &bytes_read, portMAX_DELAY);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to read from microphone: %s", esp_err_to_name(err));
        }

        // Apply AGC if enabled
        if (agc_mode_ > 0)
        {
            applyAGC();
        }

        return bytes_read;
    }

    /**
 * @brief Get the frequency spectrum of the audio data
 * @param min_freq Minimum frequency bin to include (default: 0)
 * @param max_freq Maximum frequency bin to include (default: 63)
 * @return Array of length 64 containing magnitude of each frequency bin
 */
    SpectrumBuffer getFrequencySpectrum(int min_freq = 0, int max_freq = SPECTRUM_SIZE - 1)
    {
        // Ensure parameters are in valid range
        min_freq = std::clamp(min_freq, 0, static_cast<int>(SPECTRUM_SIZE) - 1);
        max_freq = std::clamp(max_freq, min_freq, static_cast<int>(SPECTRUM_SIZE) - 1);

        // Ensure we have fresh audio data
        read();

        // Initialize the output spectrum buffer with zeros
        SpectrumBuffer spectrum = {};

        // We need at least 2*SPECTRUM_SIZE samples for a meaningful FFT
        if (buffer_.size() < 2 * SPECTRUM_SIZE)
        {
            ESP_LOGW(TAG, "Buffer too small for spectrum analysis");
            return spectrum;
        }

        // Create buffer for complex FFT input
        std::vector<std::complex<float>> fft_input(buffer_.size());

        // Apply a Hanning window function to reduce spectral leakage
        for (size_t i = 0; i < buffer_.size(); i++)
        {
            const float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / buffer_.size()));
            // Normalize input from 24-bit samples (shift by 16)
            const float sample = static_cast<float>(buffer_[i] >> 16) / 32768.0f;
            fft_input[i] = std::complex<float>(sample * window, 0.0f);
        }

        // Perform FFT (in-place)
        util::fft(fft_input);

        // Calculate magnitude spectrum
        // We only use the first half of FFT result (second half is mirrored for real input)
        const size_t fft_output_size = buffer_.size() / 2;

        // First, calculate the magnitude for all FFT bins
        std::vector<float> rawMagnitudes(fft_output_size);
        for (size_t i = 0; i < fft_output_size; i++)
        {
            rawMagnitudes[i] = std::abs(fft_input[i]);
        }

        // Now map the requested frequency range to the output spectrum using linear interpolation
        const float rangeSize = static_cast<float>(max_freq - min_freq + 1);
        const float fftBinSize = static_cast<float>(fft_output_size) / SPECTRUM_SIZE;

        for (int i = 0; i < SPECTRUM_SIZE; i++)
        {
            // Calculate the position in the original spectrum (linear mapping)
            const float normalizedPos = static_cast<float>(i) / SPECTRUM_SIZE;
            const float sourceBinFloat = min_freq + normalizedPos * rangeSize;

            // Convert to actual FFT bin position
            const float fftBinFloat = sourceBinFloat * fftBinSize;
            const int fftBinLow = static_cast<int>(fftBinFloat);
            const int fftBinHigh = fftBinLow + 1;

            // Interpolation factor
            const float alpha = fftBinFloat - fftBinLow;

            // Get magnitude using linear interpolation (with bounds checking)
            float magnitude = 0.0f;
            if (fftBinLow >= 0 && fftBinLow < fft_output_size)
            {
                magnitude += rawMagnitudes[fftBinLow] * (1.0f - alpha);
            }
            if (fftBinHigh >= 0 && fftBinHigh < fft_output_size)
            {
                magnitude += rawMagnitudes[fftBinHigh] * alpha;
            }

            // Store the magnitude
            spectrum[i] = magnitude;
        }

        // Apply some smoothing between adjacent frequency bins to avoid zeros
        for (int i = 1; i < SPECTRUM_SIZE - 1; i++)
        {
            if (spectrum[i] == 0.0f)
            {
                // If a bin is zero, interpolate from adjacent non-zero bins
                float left = spectrum[i - 1];

                // Find next non-zero bin to the right
                int rightIndex = i + 1;
                while (rightIndex < SPECTRUM_SIZE && spectrum[rightIndex] == 0.0f)
                {
                    rightIndex++;
                }

                float right = (rightIndex < SPECTRUM_SIZE) ? spectrum[rightIndex] : left;

                // Interpolate
                if (left > 0.0f || right > 0.0f)
                {
                    spectrum[i] = (left + right) * 0.5f;
                }
            }
        }

        // Normalize the spectrum for 0-64 range
        // Find the maximum value in the spectrum
        float max_value = 0.0f;
        for (size_t i = 0; i < SPECTRUM_SIZE; i++)
        {
            max_value = std::max(max_value, spectrum[i]);
        }

        // Scale to 0-64 range
        if (max_value > 0.0f)
        {
            for (size_t i = 0; i < SPECTRUM_SIZE; i++)
            {
                spectrum[i] = (spectrum[i] / max_value) * 64.0f;
            }
        }

        return spectrum;
    }

    /**
     * @brief Get the current sound level
     * @return Float value representing sound level
     */
    [[nodiscard]] float getSoundLevel() const
    {
        return _mic_level;
    }

    /**
     * @brief Set the AGC mode
     * @param mode 0 - none, 1 - normal, 2 - vivid, 3 - lazy
     */
    void setAGCMode(const uint8_t mode)
    {
        agc_mode_ = (mode > 3) ? 0 : mode;
    }

    /**
     * @brief Get the current AGC mode
     * @return Current AGC mode (0-3)
     */
    [[nodiscard]] uint8_t getAGCMode() const
    {
        return agc_mode_;
    }

private:
    static constexpr auto TAG = "Microphone";
    friend class util::Singleton<Microphone>;

    // I2S configuration
    static constexpr i2s_port_t I2S_PORT = I2S_NUM_1;
    static constexpr uint32_t SAMPLE_RATE = 16000;
    static constexpr int I2S_BUFFER_SIZE = 2048; // Keep the original buffer size
    static constexpr i2s_data_bit_width_t DATA_BITS = I2S_DATA_BIT_WIDTH_24BIT;
    static constexpr i2s_slot_bit_width_t SLOT_BITS = I2S_SLOT_BIT_WIDTH_32BIT;
    static constexpr i2s_std_slot_mask_t MIC_SLOT_MASK = I2S_STD_SLOT_LEFT;

    // GPIO pins
    struct Pin
    {
        static constexpr gpio_num_t WS = GPIO_NUM_15; // Word Select (L/R Clock)
        static constexpr gpio_num_t SCK = GPIO_NUM_14; // Serial Clock
        static constexpr gpio_num_t SD = GPIO_NUM_32; // Serial Data
    };

    // Channel handle
    i2s_chan_handle_t _rx_chan;

    // Sample buffer
    SampleBuffer buffer_{DEFAULT_BUFFER_SIZE, 0};

    // Audio processing
    uint8_t agc_mode_ = 2; // Automagic gain control: 0-none, 1-normal, 2-vivid, 3-lazy
    float _mic_level = 0.0f;

    /**
     * @brief Apply Automatic Gain Control to the buffer
     */
    void applyAGC()
    {
        if (buffer_.empty()) return;

        // Calculate mean amplitude
        float mean = 0.0f;
        for (const auto& sample : buffer_)
        {
            // Convert 24-bit data (stored in 32-bit int) to useful range
            // Using the same bit-shift as the original working code
            mean += std::abs(sample >> 16);
        }
        mean /= buffer_.size();

        // Apply different AGC algorithms based on mode
        switch (agc_mode_)
        {
        case 1: // Normal - gradual adjustment
            _mic_level = 0.9f * _mic_level + 0.1f * mean;
            break;
        case 2: // Vivid - faster adjustment
            _mic_level = 0.7f * _mic_level + 0.3f * mean;
            break;
        case 3: // Lazy - very slow adjustment
            _mic_level = 0.97f * _mic_level + 0.03f * mean;
            break;
        default:
            _mic_level = mean;
        }
    }

    /**
     * @brief Constructor - initializes the I2S interface for the INMP441
     */
    Microphone()
    {
        ESP_LOGI(TAG, "Initializing I2S microphone with STD driver");

        // 1. Configure the I2S channel - using exact same configuration as original
        constexpr i2s_chan_config_t chan_cfg = {
            .id = I2S_PORT,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = 8,
            .dma_frame_num = 128,
            .auto_clear = false,
            .auto_clear_before_cb = false,
            .allow_pd = false,
            .intr_priority = 0,
        };

        esp_err_t err = i2s_new_channel(&chan_cfg, nullptr, &_rx_chan);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to create I2S RX channel: %s", esp_err_to_name(err));
            return;
        }

        // 2. Configure I2S standard mode - using exact same configuration as original
        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = SAMPLE_RATE,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_384,
            },
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(DATA_BITS, I2S_SLOT_MODE_MONO),
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

        // INMP441 specific configuration
        std_cfg.slot_cfg.slot_bit_width = SLOT_BITS;
        std_cfg.slot_cfg.slot_mask = MIC_SLOT_MASK;

        // Initialize the channel
        err = i2s_channel_init_std_mode(_rx_chan, &std_cfg);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to initialize I2S RX channel in STD mode: %s", esp_err_to_name(err));
            i2s_del_channel(_rx_chan);
            return;
        }

        // 3. Enable the channel
        err = i2s_channel_enable(_rx_chan);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to enable I2S RX channel: %s", esp_err_to_name(err));
            i2s_channel_disable(_rx_chan);
            i2s_del_channel(_rx_chan);
            return;
        }

        ESP_LOGI(TAG, "I2S RX channel initialized and enabled successfully");

        // Test reading to confirm microphone is working (using same approach as original)
        testMicrophone();
    }

    /**
     * @brief Test if the microphone is working correctly
     * Using exact same approach as the original working code
     */
    void testMicrophone()
    {
        const size_t bytesRead = read();

        if (const int samplesRead = bytesRead / 4; samplesRead == DEFAULT_BUFFER_SIZE)
        {
            float mean = 0.0;
            ESP_LOGI(TAG, "Read the expected %d samples", DEFAULT_BUFFER_SIZE);

            for (int i = 0; i < samplesRead; i++)
            {
                mean += abs(buffer_[i] >> 16);
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

    /**
     * @brief Destructor - clean up I2S resources
     */
    ~Microphone() override
    {
        ESP_LOGI(TAG, "Shutting down microphone");
        if (_rx_chan)
        {
            i2s_channel_disable(_rx_chan);
            i2s_del_channel(_rx_chan);
        }
    }
};
