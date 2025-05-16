#pragma once

#include <cmath>
#include <array>
#include <nlohmann/json.hpp>

#include "Totem.hpp"
#include "Microphone.hpp"
#include "util/Math.h"

class AudioSpectrumPattern final : public PatternBase
{
    static constexpr float DEFAULT_PEAK_HOLD_TIME = 3.0f;
    static constexpr float DEFAULT_BAND_NORM_FACTOR = 0.995f;
    static constexpr float DEFAULT_LOG_SCALE_BASE = 8.0f;
    static constexpr float DEFAULT_ANIMATION_SPEED = 0.0025f;
    static constexpr float DEFAULT_ENERGY_ATTACK_FACTOR = 10.0f;
    static constexpr float DEFAULT_ENERGY_ATTACK_MIN = 0.2f;
    static constexpr float DEFAULT_ENERGY_ATTACK_MAX = 0.9f;
    static constexpr float DEFAULT_ENERGY_DECAY_FACTOR = 0.15f;
    static constexpr float DEFAULT_ENERGY_DECAY_MIN = 0.6f;
    static constexpr float DEFAULT_ENERGY_DECAY_MAX = 0.95f;

    float PEAK_HOLD_TIME;
    float BAND_NORM_FACTOR;
    float LOG_SCALE_BASE;
    float ANIMATION_SPEED;
    float ENERGY_ATTACK_FACTOR;
    float ENERGY_ATTACK_MIN;
    float ENERGY_ATTACK_MAX;
    float ENERGY_DECAY_FACTOR;
    float ENERGY_DECAY_MIN;
    float ENERGY_DECAY_MAX;

    using Spectrum = std::array<float, MatrixDriver::WIDTH>;
    Spectrum spectrum_{};
    Spectrum lastSpectrum_{};
    Spectrum peakLevels_{};
    Spectrum peakHoldCounters_{};
    Spectrum bandMaxHistory_{};
    Spectrum bandActivity_{};

    float dyn_attack_ = 1.0f;
    float dyn_decay_ = 1.0f;

    float animationPhase_ = 0.0f;
    bool animationDirectionForward_ = true;

public:
    explicit AudioSpectrumPattern(
        const float peak_hold_time = DEFAULT_PEAK_HOLD_TIME,
        const float band_normalization_factor = DEFAULT_BAND_NORM_FACTOR,
        const float log_scale_base = DEFAULT_LOG_SCALE_BASE,
        const float animation_speed = DEFAULT_ANIMATION_SPEED,
        const float energy_attack_factor = DEFAULT_ENERGY_ATTACK_FACTOR,
        const float energy_attack_min = DEFAULT_ENERGY_ATTACK_MIN,
        const float energy_attack_max = DEFAULT_ENERGY_ATTACK_MAX,
        const float energy_decay_factor = DEFAULT_ENERGY_DECAY_FACTOR,
        const float energy_decay_min = DEFAULT_ENERGY_DECAY_MIN,
        const float energy_decay_max = DEFAULT_ENERGY_DECAY_MAX)
        : PatternBase("AudioSpectrumPattern"),
          PEAK_HOLD_TIME(peak_hold_time),
          BAND_NORM_FACTOR(band_normalization_factor),
          LOG_SCALE_BASE(log_scale_base),
          ANIMATION_SPEED(animation_speed),
          ENERGY_ATTACK_FACTOR(energy_attack_factor),
          ENERGY_ATTACK_MIN(energy_attack_min),
          ENERGY_ATTACK_MAX(energy_attack_max),
          ENERGY_DECAY_FACTOR(energy_decay_factor),
          ENERGY_DECAY_MIN(energy_decay_min),
          ENERGY_DECAY_MAX(energy_decay_max)
    {
    }

    void from_json(const nlohmann::basic_json<>& j) override
    {
        PEAK_HOLD_TIME = j.value("peak_hold_time", DEFAULT_PEAK_HOLD_TIME);
        BAND_NORM_FACTOR = j.value("band_norm_factor", DEFAULT_BAND_NORM_FACTOR);
        LOG_SCALE_BASE = j.value("log_scale_base", DEFAULT_LOG_SCALE_BASE);
        ANIMATION_SPEED = j.value("anim_speed", DEFAULT_ANIMATION_SPEED);
        ENERGY_ATTACK_FACTOR = j.value("energy_attack_factor", DEFAULT_ENERGY_ATTACK_FACTOR);
        ENERGY_ATTACK_MIN = j.value("energy_attack_min", DEFAULT_ENERGY_ATTACK_MIN);
        ENERGY_ATTACK_MAX = j.value("energy_attack_max", DEFAULT_ENERGY_ATTACK_MAX);
        ENERGY_DECAY_FACTOR = j.value("energy_decay_factor", DEFAULT_ENERGY_DECAY_FACTOR);
        ENERGY_DECAY_MIN = j.value("energy_decay_min", DEFAULT_ENERGY_DECAY_MIN);
        ENERGY_DECAY_MAX = j.value("energy_decay_max", DEFAULT_ENERGY_DECAY_MAX);
    }

    void render() override
    {
        Microphone::getSpectrum(spectrum_);

        float energy = 0;

        // Apply logarithmic scaling and per-band normalization

        for (size_t i = 0; i < MatrixDriver::WIDTH; i++)
        {
            // Apply compression
            const float scaledValue = 1.0f + spectrum_[i] * LOG_SCALE_BASE;
            spectrum_[i] = logf(scaledValue) / logf(1.0f + LOG_SCALE_BASE);

            // Update band max history and normalize
            bandMaxHistory_[i] = std::max(bandMaxHistory_[i] * BAND_NORM_FACTOR, spectrum_[i]);
            const float normFactor = std::max(0.01f, bandMaxHistory_[i]);
            spectrum_[i] = spectrum_[i] / normFactor;

            // Scale to matrix height
            spectrum_[i] *= MatrixDriver::HEIGHT;
            energy += spectrum_[i];
        }

        // Apply energy

        energy /= MatrixDriver::WIDTH;
        dyn_attack_ = 1.0f + energy * ENERGY_ATTACK_FACTOR;
        dyn_decay_ = 1.0f - energy * ENERGY_DECAY_FACTOR;
        dyn_attack_ = std::min(std::max(dyn_attack_, ENERGY_ATTACK_MIN), ENERGY_ATTACK_MAX);
        dyn_decay_ = std::min(std::max(dyn_decay_, ENERGY_DECAY_MIN), ENERGY_DECAY_MAX);

        // Update animation
        updateAnimation();
            }
            
            // Separated animation update function to allow partial renders
            void updateAnimation() 
            {
        // Display

        if (animationDirectionForward_)
        {
            animationPhase_ += ANIMATION_SPEED;
            if (animationPhase_ >= 1.0f)
            {
                animationPhase_ = 1.0f;
                animationDirectionForward_ = false;
            }
        }
        else
        {
            animationPhase_ -= ANIMATION_SPEED;
            if (animationPhase_ <= 0.0f)
            {
                animationPhase_ = 0.0f;
                animationDirectionForward_ = true;
            }
        }

        for (uint8_t x = 0; x < MatrixDriver::WIDTH; ++x)
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

            constexpr size_t m_height_size_t = MatrixDriver::HEIGHT;
            const size_t height = std::min(static_cast<size_t>(lastSpectrum_[x]), m_height_size_t);
            const size_t peakHeight = std::min(static_cast<size_t>(peakLevels_[x]), m_height_size_t - 1);

            for (uint8_t y = 0; y < height; ++y)
            {
                using namespace util::math;
                using namespace util::colors;
                const float norm = unit_norm(y, m_height_size_t);
                const float bottom_hue = unit_lerp(GREEN, MAGENTA, animationPhase_);
                const float hue = unit_lerp(bottom_hue, RED, norm);
                draw_pixel_hsv(x, m_height_size_t - 1 - y, hue);
            }

            if (peakHeight > 0)
            {
                draw_pixel_rgb(x, m_height_size_t - 1 - peakHeight, 255, 255, 255);
            }
        }
    }
};
