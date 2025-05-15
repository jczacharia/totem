#pragma once

#include "Totem.hpp"

class SolidColorPattern final : public PatternBase
{
    static constexpr auto TAG = "SolidColorPattern";

    uint8_t red_;
    uint8_t green_;
    uint8_t blue_;

public:
    explicit SolidColorPattern(
        const uint8_t r = 0,
        const uint8_t g = 0,
        const uint8_t b = 0)
        : red_(r),
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
        totem->set_state<SolidColorPattern>(
            j.value("red", 0),
            j.value("green", 0),
            j.value("blue", 0));

        httpd_resp_sendstr(req, "SolidColorPattern set successfully");
        return ESP_OK;
    }

    void render(MatrixGfx& gfx) override
    {
        gfx.fill_rgb(red_, green_, blue_);
    }
};
