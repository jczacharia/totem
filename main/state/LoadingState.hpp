#pragma once

#include <nlohmann/json.hpp>

#include "Totem.hpp"
#include "TotemState.hpp"

class LoadingState final : public TotemState
{
    static constexpr auto TAG = "LoadingState";

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
    explicit LoadingState(
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
        auto body_or_error = util::readRequestBody(req, TAG);
        if (!body_or_error) return body_or_error.error();

        const auto j = nlohmann::json::parse(body_or_error.value());

        const auto totem = static_cast<Totem*>(req->user_ctx);
        totem->setState<LoadingState>(
            j.value("center_x", DEFAULT_CENTER_X),
            j.value("center_y", DEFAULT_CENTER_Y),
            j.value("diameter", DEFAULT_DIAMETER),
            j.value("trail_length", DEFAULT_TRAIL_LENGTH),
            j.value("positions", DEFAULT_POSITIONS));

        httpd_resp_sendstr(req, "Loading state set successfully");
        return ESP_OK;
    }

    void render(MatrixGfx& gfx) override
    {
        for (uint8_t i = 0; i < TRAIL_LENGTH; i++)
        {
            const uint8_t pos = (position_ + POSITIONS - i) % POSITIONS;

            uint8_t r, g, b;
            calcRainbowColor(i, r, g, b);

            const uint8_t trail_brightness = calcTrailBrightness(i);

            r = static_cast<uint8_t>((static_cast<uint16_t>(r) * trail_brightness / 255));
            g = static_cast<uint8_t>((static_cast<uint16_t>(g) * trail_brightness / 255));
            b = static_cast<uint8_t>((static_cast<uint16_t>(b) * trail_brightness / 255));

            uint8_t x, y;
            calcPoint(pos, x, y);

            gfx.drawPixel(x, y, r, g, b);
        }

        position_ = (position_ + 1) % POSITIONS;
    }

private:
    void calcPoint(const uint8_t position, uint8_t& x, uint8_t& y) const
    {
        const float angle = position * 2.0f * M_PI / POSITIONS;
        x = CENTER_X + static_cast<uint8_t>(DIAMETER * cos(angle));
        y = CENTER_Y + static_cast<uint8_t>(DIAMETER * sin(angle));
    }

    void calcRainbowColor(const uint8_t trail_position, uint8_t& r, uint8_t& g, uint8_t& b) const
    {
        if (const float hue = static_cast<float>(trail_position) / (TRAIL_LENGTH - 1); hue < 0.1f)
        {
            // Violet
            r = 148;
            g = 0;
            b = 211;
        }
        else if (hue < 0.25f)
        {
            // Indigo -> Blue
            const float t = (hue - 0.1f) / 0.15f;
            r = static_cast<uint8_t>(148 * (1.0f - t));
            g = 0;
            b = static_cast<uint8_t>(211 + t * (255 - 211));
        }
        else if (hue < 0.4f)
        {
            // Blue -> Cyan
            const float t = (hue - 0.25f) / 0.15f;
            r = 0;
            g = static_cast<uint8_t>(255 * t);
            b = 255;
        }
        else if (hue < 0.55f)
        {
            // Cyan -> Green
            const float t = (hue - 0.4f) / 0.15f;
            r = 0;
            g = 255;
            b = static_cast<uint8_t>(255 * (1.0f - t));
        }
        else if (hue < 0.7f)
        {
            // Green -> Yellow
            const float t = (hue - 0.55f) / 0.15f;
            r = static_cast<uint8_t>(255 * t);
            g = 255;
            b = 0;
        }
        else if (hue < 0.85f)
        {
            // Yellow -> Orange
            const float t = (hue - 0.7f) / 0.15f;
            r = 255;
            g = static_cast<uint8_t>(255 * (1.0f - t * 0.5f)); // Orange still has some green
            b = 0;
        }
        else
        {
            // Orange -> Red
            const float t = (hue - 0.85f) / 0.15f;
            r = 255;
            g = static_cast<uint8_t>(128 * (1.0f - t)); // Fade remaining green to zero
            b = 0;
        }
    }

    uint8_t calcTrailBrightness(const uint8_t distance) const
    {
        // If the distance is greater than trail length, return 0 (no brightness)
        if (distance >= TRAIL_LENGTH) return 0;
        // Linear fade from 100% at head to 40% at the end of the trail
        // The 40% minimum ensures the tail colors remain visible and vibrant
        constexpr uint8_t min_brightness = 100;
        return min_brightness + static_cast<uint8_t>(
            (255 - min_brightness) * (TRAIL_LENGTH - distance) / TRAIL_LENGTH);
    }
};
