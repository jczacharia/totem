#pragma once

#include "TotemState.hpp"

class SolidColorState final : public TotemState
{
    uint8_t red_;
    uint8_t green_;
    uint8_t blue_;

public:
    SolidColorState(const uint8_t r, const uint8_t g, const uint8_t b)
        : red_(r), green_(g), blue_(b)
    {
    }

    void render(MatrixGfx& gfx) override
    {
        gfx.clear();
        gfx.fillRgb(red_, green_, blue_);
        gfx.flush();
    }

    void setColor(const uint8_t r, const uint8_t g, const uint8_t b)
    {
        red_ = r;
        green_ = g;
        blue_ = b;
    }
};
