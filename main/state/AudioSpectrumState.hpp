#pragma once

#include <cmath>
#include <array>
#include <nlohmann/json.hpp>

#include "audio/Microphone.hpp"
#include "TotemState.hpp"

class AudioSpectrumState final : public TotemState
{
    static constexpr auto TAG = "AudioSpectrumState";
    Microphone& microphone_ = Microphone::getInstance();

    static constexpr float DEFAULT_PEAK_HOLD_TIME = 3.0f;
    static constexpr float DEFAULT_BAND_NORM_FACTOR = 0.995f;
    static constexpr float DEFAULT_LOG_SCALE_BASE = 8.0f;
    static constexpr float DEFAULT_ANIMATION_SPEED = 0.001f;

    float PEAK_HOLD_TIME;
    float BAND_NORM_FACTOR;
    float LOG_SCALE_BASE;
    float ANIMATION_SPEED;

    using Spectrum = std::array<float, LedMatrix::MATRIX_WIDTH>;
    Spectrum spectrum_{};
    Spectrum lastSpectrum_{};
    Spectrum peakLevels_{};
    Spectrum peakHoldCounters_{};
    Spectrum bandMaxHistory_{};
    Spectrum bandActivity_{};

    float dyn_attack_ = 1.0f;
    float dyn_decay_ = 1.0f;
    float activityDecay = 0.9f;
    float activityGain = 3.0f;

    float animationPhase_ = 0.0f;
    bool animationDirectionForward_ = true;

public:
    explicit AudioSpectrumState(
        const float peak_hold_time = DEFAULT_PEAK_HOLD_TIME,
        const float band_normalization_factor = DEFAULT_BAND_NORM_FACTOR,
        const float log_scale_base = DEFAULT_LOG_SCALE_BASE,
        const float animation_speed = DEFAULT_ANIMATION_SPEED)
        : PEAK_HOLD_TIME(peak_hold_time),
          BAND_NORM_FACTOR(band_normalization_factor),
          LOG_SCALE_BASE(log_scale_base),
          ANIMATION_SPEED(animation_speed)
    {
    }

    static esp_err_t endpoint(httpd_req_t* req)
    {
        auto body_or_error = util::readRequestBody(req, TAG);
        if (!body_or_error) return body_or_error.error();

        const auto j = nlohmann::json::parse(body_or_error.value());

        Totem::setState<AudioSpectrumState>(
            j.value("peak_hold_time", DEFAULT_PEAK_HOLD_TIME),
            j.value("band_norm_factor", DEFAULT_BAND_NORM_FACTOR),
            j.value("log_scale_base", DEFAULT_LOG_SCALE_BASE),
            j.value("anim_speed", DEFAULT_ANIMATION_SPEED));

        httpd_resp_sendstr(req, "AudioSpectrumState set successfully");
        return ESP_OK;
    }

private:
    void handleAnimPhase()
    {
        if (animationDirectionForward_)
        {
            animationPhase_ += ANIMATION_SPEED;
            if (animationPhase_ >= 0.5f)
            {
                // 0.5 represents violet in our hue range (0.33 to 0.83)
                animationPhase_ = 0.5f;
                animationDirectionForward_ = false;
            }
        }
        else
        {
            animationPhase_ -= ANIMATION_SPEED;
            if (animationPhase_ <= 0.0f)
            {
                // 0.0 represents green in our remapped range
                animationPhase_ = 0.0f;
                animationDirectionForward_ = true;
            }
        }
    }

    void getHeightColor(const int height, uint8_t& r, uint8_t& g, uint8_t& b) const
    {
        const float normalizedHeight = static_cast<float>(height) / LedMatrix::MATRIX_HEIGHT;
        const float hue = (0.33f + animationPhase_) * (1.0f - normalizedHeight);

        const float h_i = hue * 6.0f;
        const int i = static_cast<int>(h_i);
        const float f = h_i - i;
        constexpr float p = 0.0f;
        const float q = 1.0f - f;
        const float t = f;

        switch (i % 6)
        {
        case 0:
            r = 255;
            g = static_cast<uint8_t>(t * 255.0f);
            b = static_cast<uint8_t>(p * 255.0f);
            break;
        case 1:
            r = static_cast<uint8_t>(q * 255.0f);
            g = 255;
            b = static_cast<uint8_t>(p * 255.0f);
            break;
        case 2:
            r = static_cast<uint8_t>(p * 255.0f);
            g = 255;
            b = static_cast<uint8_t>(t * 255.0f);
            break;
        case 3:
            r = static_cast<uint8_t>(p * 255.0f);
            g = static_cast<uint8_t>(q * 255.0f);
            b = 255;
            break;
        case 4:
            r = static_cast<uint8_t>(t * 255.0f);
            g = static_cast<uint8_t>(p * 255.0f);
            b = 255;
            break;
        case 5:
            r = 255;
            g = static_cast<uint8_t>(p * 255.0f);
            b = static_cast<uint8_t>(q * 255.0f);
            break;
        default:
            break;
        }
    }

public:
    void render(MatrixGfx& gfx) override
    {
        handleAnimPhase();

        microphone_.getSpectrum(spectrum_);

        float energy = 0;

        // Apply logarithmic scaling and per-band normalization

        for (size_t i = 0; i < LedMatrix::MATRIX_WIDTH; i++)
        {
            // Apply compression
            const float scaledValue = 1.0f + spectrum_[i] * LOG_SCALE_BASE;
            spectrum_[i] = logf(scaledValue) / logf(1.0f + LOG_SCALE_BASE);

            // Update band max history and normalize
            bandMaxHistory_[i] = std::max(bandMaxHistory_[i] * BAND_NORM_FACTOR, spectrum_[i]);
            const float normFactor = std::max(0.01f, bandMaxHistory_[i]);
            spectrum_[i] = spectrum_[i] / normFactor;

            // Scale to matrix height
            spectrum_[i] *= LedMatrix::MATRIX_HEIGHT;
            energy += spectrum_[i];
        }

        // Apply energy

        energy /= LedMatrix::MATRIX_WIDTH;
        dyn_attack_ = 1.0f + energy;
        dyn_decay_ = 1.0f - energy * 0.2f;
        dyn_attack_ = std::min(std::max(dyn_attack_, 0.1f), 0.9f);
        dyn_decay_ = std::min(std::max(dyn_decay_, 0.7f), 0.98f);

        // Display

        for (int x = 0; x < LedMatrix::MATRIX_WIDTH; ++x)
        {
            if (const float currentValue = spectrum_[x]; currentValue > lastSpectrum_[x])
            {
                lastSpectrum_[x] = lastSpectrum_[x] * (1.0f - dyn_attack_) + currentValue * dyn_attack_;
            }
            else
            {
                lastSpectrum_[x] = lastSpectrum_[x] * dyn_decay_ + currentValue * (1.0f - dyn_decay_);
            }

            if (lastSpectrum_[x] > peakLevels_[x])
            {
                peakLevels_[x] = lastSpectrum_[x];
                peakHoldCounters_[x] = PEAK_HOLD_TIME;
            }
            else
            {
                if (peakHoldCounters_[x] > 0)
                {
                    peakHoldCounters_[x] -= 1.0f;
                }
                else
                {
                    peakLevels_[x] *= 0.9f;
                }
            }

            const size_t height = std::min(
                static_cast<size_t>(lastSpectrum_[x]), static_cast<size_t>(LedMatrix::MATRIX_HEIGHT));

            const size_t peakHeight = std::min(
                static_cast<size_t>(peakLevels_[x]), static_cast<size_t>(LedMatrix::MATRIX_HEIGHT) - 1);

            for (int y = 0; y < height; ++y)
            {
                uint8_t r, g, b;
                getHeightColor(y, r, g, b);
                gfx.drawPixel(x, LedMatrix::MATRIX_HEIGHT - 1 - y, r, g, b);
            }

            if (peakHeight > 0)
            {
                gfx.drawPixel(x, LedMatrix::MATRIX_HEIGHT - 1 - peakHeight, 255, 255, 255);
            }
        }
    }
};
