#pragma once

#include "Totem.hpp"

class SinglePixelPattern final : public PatternBase
{
    static constexpr auto TAG = "SinglePixelPattern";

    uint8_t x_;
    uint8_t y_;
    uint8_t red_;
    uint8_t green_;
    uint8_t blue_;

public:
    explicit SinglePixelPattern(
        const uint8_t x = 0,
        const uint8_t y = 0,
        const uint8_t r = 0,
        const uint8_t g = 0,
        const uint8_t b = 0)
        : x_(x),
          y_(y),
          red_(r),
          green_(g),
          blue_(b)
    {
    }

    static esp_err_t endpoint(httpd_req_t* req)
    {
        auto body_or_error = util::http::get_req_body(req, TAG);
        if (!body_or_error) return body_or_error.error();

        const auto j = nlohmann::json::parse(body_or_error.value());

        const auto totem = static_cast<Totem*>(req->user_ctx);
        totem->set_state<SinglePixelPattern>(
            j.value("x", 0),
            j.value("y", 0),
            j.value("red", 0),
            j.value("green", 0),
            j.value("blue", 0));

        httpd_resp_sendstr(req, "SinglePixelPattern set successfully");
        return ESP_OK;
    }

    void render(MatrixGfx& gfx) override
    {
        gfx.draw_pixel_rgb(x_, y_, red_, green_, blue_);
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
