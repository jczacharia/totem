#pragma once

#include <cstdint>

#include "nlohmann/json.hpp"

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

    inline void rgb_to_hsl(const uint8_t r, const uint8_t g, const uint8_t b, float& h, float& s, float& l)
    {
        // Convert RGB from [0,255] to [0,1]
        const float rf = r / 255.0f;
        const float gf = g / 255.0f;
        const float bf = b / 255.0f;

        const float max_val = std::max({rf, gf, bf});
        const float min_val = std::min({rf, gf, bf});
        const float delta = max_val - min_val;

        // Calculate lightness
        l = (max_val + min_val) / 2.0f;

        // Calculate saturation
        if (delta < 0.00001f)
        {
            h = 0.0f;
            s = 0.0f;
        }
        else
        {
            s = delta / (1.0f - std::abs(2.0f * l - 1.0f));

            // Calculate hue
            if (max_val == rf)
            {
                h = (gf - bf) / delta + (gf < bf ? 6.0f : 0.0f);
            }
            else if (max_val == gf)
            {
                h = (bf - rf) / delta + 2.0f;
            }
            else // max_val == bf
            {
                h = (rf - gf) / delta + 4.0f;
            }
            h /= 6.0f;
        }
    }
}
