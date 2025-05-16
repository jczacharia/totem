#pragma once

#include "Totem.hpp"

class SolidColorPattern final : public PatternBase
{
    uint8_t red_;
    uint8_t green_;
    uint8_t blue_;

public:
    explicit SolidColorPattern(
        const uint8_t r = 0,
        const uint8_t g = 0,
        const uint8_t b = 0)
        : PatternBase("SolidColorPattern"),
          red_(r),
          green_(g),
          blue_(b)
    {
    }

    void from_json(const nlohmann::basic_json<>& j) override
    {
        red_ = j.value("red", 0);
        green_ = j.value("green", 0);
        blue_ = j.value("blue", 0);
    }

    void render(MatrixGfx& gfx) override
    {
        gfx.fill_rgb(red_, green_, blue_);
    }
};
