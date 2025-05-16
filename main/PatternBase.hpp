#pragma once

#include <atomic>

#include "esp_http_server.h"
#include "MatrixDriver.hpp"
#include "util/Colors.hpp"

class PatternBase
{
public:
    static constexpr TickType_t DEFAULT_RENDER_SPEED_MS = 16;
    static constexpr TickType_t DEFAULT_RENDER_TICK = pdMS_TO_TICKS(DEFAULT_RENDER_SPEED_MS);
    static constexpr TickType_t MIN_RENDER_TICK = 1;

private:
    std::string name_;
    std::atomic<size_t> render_speed_{DEFAULT_RENDER_TICK};

    static void color_to_rgb_(const uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b)
    {
        r = (color >> 16) & 0xFF;
        g = (color >> 8) & 0xFF;
        b = color & 0xFF;
    }

    static uint32_t rgb_to_color_(const uint8_t red, const uint8_t green, const uint8_t blue)
    {
        const auto r = static_cast<uint32_t>(red) << 16;
        const auto g = static_cast<uint32_t>(green) << 8;
        const auto b = static_cast<uint32_t>(blue);
        return r | g | b;
    }

protected:
    explicit PatternBase(std::string name) : name_(std::move(name))
    {
    }

public:
    std::array<uint32_t, MatrixDriver::SIZE> buffer_{};


    virtual ~PatternBase() = default;

    virtual void render() = 0;

    void clear()
    {
        std::ranges::fill(buffer_, 0);
    }

    [[nodiscard]] const std::array<uint32_t, MatrixDriver::SIZE>& get_buf() const
    {
        return buffer_;
    }

    virtual void from_json(const nlohmann::basic_json<>& j)
    {
    }

    [[nodiscard]] TickType_t get_render_tick() const
    {
        return render_speed_.load();
    }

    void set_render_tick(const TickType_t tick)
    {
        render_speed_.store(std::max(tick, MIN_RENDER_TICK));
    }

    [[nodiscard]] const std::string& get_name() const
    {
        return name_;
    }

    void draw_pixel_rgb(const uint8_t x, const uint8_t y, const uint8_t r, const uint8_t g, const uint8_t b)
    {
        if (x >= MatrixDriver::WIDTH || y >= MatrixDriver::HEIGHT) return;
        buffer_[y * MatrixDriver::WIDTH + x] = rgb_to_color_(r, g, b);
    }

    void draw_pixel_hsv(const uint8_t x, const uint8_t y, const float h, const float s = 1.0f, const float v = 1.0f)
    {
        uint8_t r, g, b;
        util::colors::hsv_to_rgb(h, s, v, r, g, b);
        draw_pixel_rgb(x, y, r, g, b);
    }

    void get_pixel_rgb(const uint8_t x, const uint8_t y, uint8_t& r_out, uint8_t& g_out, uint8_t& b_out) const
    {
        if (x >= MatrixDriver::WIDTH || y >= MatrixDriver::HEIGHT)
        {
            r_out = g_out = b_out = 0;
            return;
        }

        const uint32_t color = buffer_[y * MatrixDriver::WIDTH + x];
        color_to_rgb_(color, r_out, g_out, b_out);
    }

    void get_pixel_hsl(const uint8_t x, const uint8_t y, float& h_out, float& s_out, float& l_out) const
    {
        if (x >= MatrixDriver::WIDTH || y >= MatrixDriver::HEIGHT)
        {
            h_out = s_out = l_out = 0;
            return;
        }

        uint8_t r, g, b;
        get_pixel_rgb(x, y, r, g, b);
        util::colors::rgb_to_hsl(r, g, b, h_out, s_out, l_out);
    }
};
