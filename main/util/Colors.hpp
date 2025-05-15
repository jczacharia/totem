#pragma once

#include <cstdint>

namespace util::colors
{
    static constexpr float RED = 0.0f;
    static constexpr float ORANGE = 30.0f / 360.0f;
    static constexpr float YELLOW = 60.0f / 360.0f;
    static constexpr float GREEN = 120.0f / 360.0f;
    static constexpr float CYAN = 180.0f / 360.0f;
    static constexpr float BLUE = 240.0f / 360.0f;
    static constexpr float PURPLE = 270.0f / 360.0f;
    static constexpr float MAGENTA = 300.0f / 360.0f;
    static constexpr float PINK = 330.0f / 360.0f;

    /**
     * @brief Converts HSV color values to RGB color values
     * @param h Hue value in range [0,1]
     * @param s Saturation value in range [0,1]
     * @param v Value/brightness in range [0,1]
     * @param r Output red component in range [0,255]
     * @param g Output green component in range [0,255]
     * @param b Output blue component in range [0,255]
     */
    inline void hsv_to_rgb(const float h, const float s, const float v, uint8_t& r, uint8_t& g, uint8_t& b)
    {
        // Ensure h, s, and v are all in [0, 1]
        float hue = std::fmod(h, 1.0f);
        if (hue < 0.0f) hue += 1.0f;

        const float saturation = std::clamp(s, 0.0f, 1.0f);
        const float value = std::clamp(v, 0.0f, 1.0f);

        // Edge case: gray scale when s = 0
        if (saturation <= 0.0f)
        {
            r = g = b = static_cast<uint8_t>(value * 255.0f + 0.5f);
            return;
        }

        hue *= 6.0f; // Convert to sector between 0 and 6
        const int sector = static_cast<int>(hue);
        const float fraction = hue - sector; // Fractional part

        const float p = value * (1.0f - saturation);
        const float q = value * (1.0f - saturation * fraction);
        const float t = value * (1.0f - saturation * (1.0f - fraction));

        float red, green, blue;

        switch (sector)
        {
        case 0:
            red = value;
            green = t;
            blue = p;
            break;
        case 1:
            red = q;
            green = value;
            blue = p;
            break;
        case 2:
            red = p;
            green = value;
            blue = t;
            break;
        case 3:
            red = p;
            green = q;
            blue = value;
            break;
        case 4:
            red = t;
            green = p;
            blue = value;
            break;
        default: // sector 5
            red = value;
            green = p;
            blue = q;
            break;
        }

        // Convert from [0,1] to [0,255] and round
        r = static_cast<uint8_t>(red * 255.0f + 0.5f);
        g = static_cast<uint8_t>(green * 255.0f + 0.5f);
        b = static_cast<uint8_t>(blue * 255.0f + 0.5f);
    }

    /**
     * @brief Rescales a value from [0,1] range to [min,max] range
     * @param value Value to rescale in range [0,1]
     * @param min Lower bound of target range
     * @param max Upper bound of target range
     * @return Rescaled value in range [min,max]
     */
    inline float rescale(const float value, const float min, const float max)
    {
        const float clampedValue = std::clamp(value, 0.0f, 1.0f);
        return min + clampedValue * (max - min);
    }
}
