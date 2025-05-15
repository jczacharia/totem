#pragma once

namespace util::math
{
    inline float unit_norm(const size_t index, const size_t count)
    {
        if (count <= 1) return 0.0f;
        return static_cast<float>(index) / (count - 1);
    }

    inline float unit_lerp(const float min, const float max, const float value)
    {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        return std::lerp(min, max, clamped);
    }
}
