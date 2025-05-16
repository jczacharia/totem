#pragma once

#include "Totem.hpp"

class SinglePixelPattern final : public PatternBase
{
    uint8_t x_;
    uint8_t y_;
    uint8_t red_;
    uint8_t green_;
    uint8_t blue_;

public:
    static constexpr auto NAME = "SinglePixelPattern";

    explicit SinglePixelPattern(
        const uint8_t x = 0,
        const uint8_t y = 0,
        const uint8_t r = 0,
        const uint8_t g = 0,
        const uint8_t b = 0)
        : PatternBase(NAME),
          x_(x),
          y_(y),
          red_(r),
          green_(g),
          blue_(b)
    {
    }

    void from_json(const nlohmann::basic_json<>& j) override
    {
        x_ = j.value("x", 0);
        y_ = j.value("y", 0);
        red_ = j.value("red", 0);
        green_ = j.value("green", 0);
        blue_ = j.value("blue", 0);
    }

    void render(MatrixGfx& gfx) override
    {
        gfx.draw_pixel_rgb(x_, y_, red_, green_, blue_);
    }
};
