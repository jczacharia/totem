#pragma once

#include "PatternBase.hpp"
#include <vector>
#include <memory>
#include <chrono>

class Playlist : public PatternBase
{
    // Structure to hold pattern timing information
    struct PatternInfo
    {
        std::shared_ptr<PatternBase> pattern;
        uint32_t start_time_ms; // Start time in milliseconds
        uint32_t end_time_ms; // End time in milliseconds
        uint32_t fade_ms; // Fade in/out time in milliseconds (optional)
    };

    std::vector<PatternInfo> timed_patterns;

    uint32_t total_time_ms_{60000}; // Default to 60 seconds
    uint32_t current_time_ms_{0}; // Current position in the playlist
    std::chrono::time_point<std::chrono::steady_clock> last_update_time_;
    bool time_initialized_{false};

protected:
    // Method with timing parameters in milliseconds
    template <typename TPattern, typename... Args>
    void add_pattern(uint32_t start_time_ms, uint32_t end_time_ms, uint32_t fade_ms = 0, Args&&... args)
    {
        // Ensure time values don't exceed total_time
        start_time_ms = std::min(start_time_ms, total_time_ms_);
        end_time_ms = std::min(end_time_ms, total_time_ms_);

        // Allowing end_time to be less than start_time to indicate a pattern that wraps around
        // from the end of the playlist back to the beginning

        auto pattern = std::make_shared<TPattern>(std::forward<Args>(args)...);
        timed_patterns.push_back({pattern, start_time_ms, end_time_ms, fade_ms});
    }

    // Add a pattern that spans from start_time to the end of the playlist,
    // and then from the beginning to end_time
    template <typename TPattern, typename... Args>
    void add_wraparound_pattern(uint32_t start_time_ms, uint32_t end_time_ms, uint32_t fade_ms = 0, Args&&... args)
    {
        // Ensure time values don't exceed total_time
        start_time_ms = std::min(start_time_ms, total_time_ms_);
        end_time_ms = std::min(end_time_ms, total_time_ms_);

        auto pattern = std::make_shared<TPattern>(std::forward<Args>(args)...);

        // For clarity in the implementation, we'll treat this as a special case
        // where end_time < start_time
        timed_patterns.push_back({pattern, start_time_ms, end_time_ms, fade_ms});
    }

    void set_total_time(const uint32_t time_ms)
    {
        total_time_ms_ = time_ms;
    }

    void set_start_time(const uint32_t start_time_ms)
    {
        // Ensure start time doesn't exceed total time
        current_time_ms_ = std::min(start_time_ms, total_time_ms_);
        
        // We need to reset the time initialization to avoid a big time jump
        // on the first update_time() call
        time_initialized_ = false;
    }
    
    // Update current playlist time
    void update_time()
    {
        const auto now = std::chrono::steady_clock::now();

        if (!time_initialized_)
        {
            last_update_time_ = now;
            time_initialized_ = true;
            return;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_update_time_).count();
        last_update_time_ = now;

        current_time_ms_ += static_cast<uint32_t>(elapsed);
        if (current_time_ms_ >= total_time_ms_)
        {
            current_time_ms_ %= total_time_ms_;
        }
    }

    // Calculate pattern weight based on current time and fade settings
    float calculate_pattern_weight(const PatternInfo& pattern_info) const
    {
        // Special handling for patterns that wrap around the playlist boundary
        const bool is_wraparound_pattern = pattern_info.end_time_ms < pattern_info.start_time_ms;

        // Check if pattern is active at current time
        bool is_active;
        if (is_wraparound_pattern)
        {
            // For patterns that wrap around (end < start), they're active when time is
            // either after start OR before end
            is_active = current_time_ms_ >= pattern_info.start_time_ms ||
                current_time_ms_ <= pattern_info.end_time_ms;
        }
        else
        {
            // Normal case: pattern active between start and end times
            is_active = current_time_ms_ >= pattern_info.start_time_ms &&
                current_time_ms_ <= pattern_info.end_time_ms;
        }

        if (!is_active)
        {
            // Check if we're in a transition between playlist loops
            // Handle case where pattern ends at the end of the playlist and the next pattern
            // starts at the beginning
            if (pattern_info.end_time_ms == total_time_ms_ && pattern_info.fade_ms > 0)
            {
                // If the current time is at the start of the playlist and within fade range
                if (current_time_ms_ < pattern_info.fade_ms)
                {
                    // Calculate fade progress (from end of playlist back to beginning)
                    const float progress = static_cast<float>(pattern_info.fade_ms - current_time_ms_) /
                        static_cast<float>(pattern_info.fade_ms);
                    return progress;
                }
            }

            // If it's not active and not in a transition, return zero weight
            return 0.0f;
        }

        // If no fade, return full weight when active
        if (pattern_info.fade_ms == 0)
        {
            return 1.0f;
        }

        // Calculate fade in
        if (is_wraparound_pattern)
        {
            // Handle fade-in for wraparound patterns
            if (current_time_ms_ >= pattern_info.start_time_ms &&
                current_time_ms_ < pattern_info.start_time_ms + pattern_info.fade_ms)
            {
                const float progress = static_cast<float>(current_time_ms_ - pattern_info.start_time_ms) /
                    static_cast<float>(pattern_info.fade_ms);
                return progress;
            }
        }
        else if (current_time_ms_ < pattern_info.start_time_ms + pattern_info.fade_ms)
        {
            const float progress = static_cast<float>(current_time_ms_ - pattern_info.start_time_ms) /
                static_cast<float>(pattern_info.fade_ms);
            return progress;
        }

        // Calculate fade out
        if (is_wraparound_pattern)
        {
            // Handle fade-out for wraparound patterns
            if (current_time_ms_ <= pattern_info.end_time_ms &&
                current_time_ms_ > pattern_info.end_time_ms - pattern_info.fade_ms)
            {
                const float progress = static_cast<float>(pattern_info.end_time_ms - current_time_ms_) /
                    static_cast<float>(pattern_info.fade_ms);
                return progress;
            }

            // Also handle special fade-out near the end of the playlist for wraparound patterns
            if (current_time_ms_ > total_time_ms_ - pattern_info.fade_ms)
            {
                const float progress = static_cast<float>(total_time_ms_ - current_time_ms_) /
                    static_cast<float>(pattern_info.fade_ms);
                return 1.0f - (1.0f - progress) * (pattern_info.fade_ms /
                    static_cast<float>(std::min(pattern_info.fade_ms, total_time_ms_ - pattern_info.start_time_ms)));
            }
        }
        else if (current_time_ms_ > pattern_info.end_time_ms - pattern_info.fade_ms)
        {
            const float progress = static_cast<float>(pattern_info.end_time_ms - current_time_ms_) /
                static_cast<float>(pattern_info.fade_ms);
            return progress;
        }

        // Fully visible
        return 1.0f;
    }

public:
    explicit Playlist(std::string name) : PatternBase(std::move(name)),
                                          last_update_time_(std::chrono::steady_clock::now())
    {
    }

    void render() override
    {
        // Update current time in the playlist
        update_time();

        // If using timed patterns
        if (!timed_patterns.empty())
        {
            float total_weight = 0.0f;
            std::vector<float> weights;
            weights.reserve(timed_patterns.size());

            // Calculate and collect weights for all patterns
            for (const auto& pattern_info : timed_patterns)
            {
                pattern_info.pattern->clear();
                pattern_info.pattern->render();

                float weight = calculate_pattern_weight(pattern_info);
                weights.push_back(weight);
                total_weight += weight;
            }

            // If no active patterns, return
            if (total_weight <= 0.0f) return;

            // Blend patterns
            for (size_t i = 0; i < MatrixDriver::SIZE; i++)
            {
                float r_sum = 0.0f;
                float g_sum = 0.0f;
                float b_sum = 0.0f;

                for (size_t j = 0; j < timed_patterns.size(); j++)
                {
                    const float weight = weights[j];
                    if (weight <= 0.0f) continue;

                    const PatternBase* source = timed_patterns[j].pattern.get();
                    const uint32_t color = source->buffer_[i];

                    const float r = static_cast<float>((color >> 16) & 0xFF);
                    const float g = static_cast<float>((color >> 8) & 0xFF);
                    const float b = static_cast<float>(color & 0xFF);

                    const float norm_weight = weight / total_weight;
                    r_sum += r * norm_weight;
                    g_sum += g * norm_weight;
                    b_sum += b * norm_weight;
                }

                const uint32_t r = static_cast<uint32_t>(r_sum + 0.5f);
                const uint32_t g = static_cast<uint32_t>(g_sum + 0.5f);
                const uint32_t b = static_cast<uint32_t>(b_sum + 0.5f);

                buffer_[i] = (r << 16) | (g << 8) | b;
            }
        }
    }
};
