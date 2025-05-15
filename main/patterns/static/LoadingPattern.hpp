#pragma once

#include <nlohmann/json.hpp>

#include "Totem.hpp"
#include "util/Math.h"

class LoadingPattern final : public PatternBase
{
    static constexpr auto TAG = "LoadingPattern";

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
        const uint8_t positions = 32):
        CENTER_X(center_x),
        CENTER_Y(center_y),
        DIAMETER(diameter),
        TRAIL_LENGTH(trail_length),
        POSITIONS(positions)
    {
        setRenderTick(pdMS_TO_TICKS(33));
    }

    static esp_err_t endpoint(httpd_req_t* req)
    {
        auto body_or_error = util::http::get_req_body(req, TAG);
        if (!body_or_error) return body_or_error.error();

        const auto j = nlohmann::json::parse(body_or_error.value());

        const auto totem = static_cast<Totem*>(req->user_ctx);
        totem->set_state<LoadingPattern>(
            j.value("center_x", DEFAULT_CENTER_X),
            j.value("center_y", DEFAULT_CENTER_Y),
            j.value("diameter", DEFAULT_DIAMETER),
            j.value("trail_length", DEFAULT_TRAIL_LENGTH),
            j.value("positions", DEFAULT_POSITIONS));

        httpd_resp_sendstr(req, "LoadingPattern set successfully");
        return ESP_OK;
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
            const float hue = util::math::unit_lerp(util::colors::PURPLE, util::colors::RED, norm);
            const float brightness = util::math::unit_lerp(1.0f, 0.5f, norm);

            gfx.draw_pixel_hsv(x, y, hue, 1.0f, brightness);
        }

        position_ = (position_ + 1) % POSITIONS;
    }
};
