#pragma once

#include <nlohmann/json.hpp>

#include "Totem.hpp"
#include "util/Math.h"

class LoadingPattern final : public PatternBase
{
    static constexpr uint8_t DEFAULT_CENTER_X = 32;
    static constexpr uint8_t DEFAULT_CENTER_Y = 32;
    static constexpr uint8_t DEFAULT_DIAMETER = 20;
    static constexpr uint8_t DEFAULT_TRAIL_LENGTH = 18;
    static constexpr uint8_t DEFAULT_POSITIONS = 32;

    uint8_t CENTER_X;
    uint8_t CENTER_Y;
    uint8_t DIAMETER;
    uint8_t TRAIL_LENGTH;
    uint8_t POSITIONS;

    uint8_t position_ = 0;

public:
    explicit LoadingPattern(
        const uint8_t center_x = 32,
        const uint8_t center_y = 32,
        const uint8_t diameter = 20,
        const uint8_t trail_length = 18,
        const uint8_t positions = 32)
        : PatternBase("LoadingPattern"),
          CENTER_X(center_x),
          CENTER_Y(center_y),
          DIAMETER(diameter),
          TRAIL_LENGTH(trail_length),
          POSITIONS(positions)
    {
        set_render_tick(pdMS_TO_TICKS(33));
    }

    void from_json(const nlohmann::basic_json<>& j) override
    {
        CENTER_X = j.value("center_x", DEFAULT_CENTER_X);
        CENTER_Y = j.value("center_y", DEFAULT_CENTER_Y);
        DIAMETER = j.value("diameter", DEFAULT_DIAMETER);
        TRAIL_LENGTH = j.value("trail_length", DEFAULT_TRAIL_LENGTH);
        POSITIONS = j.value("positions", DEFAULT_POSITIONS);
    }

    void render(MatrixGfx& gfx) override
    {
        for (uint8_t i = 0; i < TRAIL_LENGTH; i++)
        {
            const uint8_t position = (position_ + POSITIONS - i) % POSITIONS;
            const float angle = position * 2.0f * M_PI / POSITIONS;
            const uint8_t x = CENTER_X + static_cast<uint8_t>(DIAMETER * cos(angle));
            const uint8_t y = CENTER_Y + static_cast<uint8_t>(DIAMETER * sin(angle));

            const float norm = util::math::unit_norm(i, TRAIL_LENGTH);
            const float hue = util::math::unit_lerp(util::colors::MAGENTA, util::colors::RED, norm);
            const float brightness = util::math::unit_lerp(1.0f, 0.5f, norm);

            gfx.draw_pixel_hsv(x, y, hue, 1.0f, brightness);
        }

        position_ = (position_ + 1) % POSITIONS;
    }
};
