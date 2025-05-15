#pragma once

#include "TotemState.hpp"

class SinglePixelState final : public TotemState
{
    uint8_t x_;
    uint8_t y_;
    uint8_t red_;
    uint8_t green_;
    uint8_t blue_;

public:
    SinglePixelState(const uint8_t x, const uint8_t y, const uint8_t r, const uint8_t g, const uint8_t b)
        : x_(x), y_(y), red_(r), green_(g), blue_(b)
    {
    }

    void render(MatrixGfx& gfx) override
    {
        gfx.clear();
        gfx.drawPixel(x_, y_, red_, green_, blue_);
        gfx.flush();
    }

    void setPosition(const uint8_t x, const uint8_t y)
    {
        x_ = x;
        y_ = y;
    }

    void setColor(const uint8_t r, const uint8_t g, const uint8_t b)
    {
        red_ = r;
        green_ = g;
        blue_ = b;
    }
};
