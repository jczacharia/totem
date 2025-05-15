#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "util/Font.hpp"

#include "LedMatrix.hpp"
#include "audio/Microphone.hpp"

class MatrixGfx final
{
public:
    static constexpr uint8_t FONT_WIDTH = 5;
    static constexpr uint8_t FONT_HEIGHT = 7;
    static constexpr uint8_t FONT_SPACING = 1;
    friend class Totem;

    Microphone& microphone;

private:
    LedMatrix& led_matrix_;

    // Internal frame buffer to store pixel data (0x00RRGGBB)
    std::array<uint32_t, LedMatrix::MATRIX_SIZE> buffer_{};

    uint8_t cursor_x_ = 0;
    uint8_t cursor_y_ = 0;

    uint8_t brightness_ = 255;

    static constexpr const uint8_t* font5x7 = util::font5x7;
    uint8_t text_size_ = 1;
    bool wrap_text_ = true;

    static void colorToRgb(const uint32_t color, uint8_t& r, uint8_t& g, uint8_t& b)
    {
        r = (color >> 16) & 0xFF;
        g = (color >> 8) & 0xFF;
        b = color & 0xFF;
    }

    static uint32_t rgbToColor(const uint8_t red, const uint8_t green, const uint8_t blue)
    {
        const auto r = static_cast<uint32_t>(red) << 16;
        const auto g = static_cast<uint32_t>(green) << 8;
        const auto b = static_cast<uint32_t>(blue);
        return r | g | b;
    }

    void clear()
    {
        std::ranges::fill(buffer_, 0);
    }

    void flush() const
    {
        led_matrix_.loadFromBuffer(buffer_);
        led_matrix_.setBrightness(brightness_);
    }

public:
    explicit MatrixGfx(LedMatrix& led_matrix, Microphone& mic)
        : microphone(mic), led_matrix_(led_matrix)
    {
    }

    void setBufferFromPtr(const uint32_t* ptr)
    {
        if (ptr != nullptr)
        {
            std::copy_n(ptr, LedMatrix::MATRIX_SIZE, buffer_.begin());
        }
    }

    void setBrightness(const uint8_t brightness)
    {
        brightness_ = brightness;
    }

    void setWrapText(const bool wrap_text)
    {
        wrap_text_ = wrap_text;
    }

    void setCursor(const uint8_t x, const uint8_t y)
    {
        cursor_x_ = std::min(x, static_cast<uint8_t>(LedMatrix::MATRIX_WIDTH - 1));
        cursor_y_ = std::min(y, static_cast<uint8_t>(LedMatrix::MATRIX_HEIGHT - 1));
    }

    void resetCursor()
    {
        cursor_x_ = 0;
        cursor_y_ = 0;
    }

    void getCursor(uint8_t& x, uint8_t& y) const
    {
        x = cursor_x_;
        y = cursor_y_;
    }

    void drawPixel(const uint8_t x, const uint8_t y, const uint8_t r, const uint8_t g, const uint8_t b)
    {
        if (x >= LedMatrix::MATRIX_WIDTH || y >= LedMatrix::MATRIX_HEIGHT) return;
        buffer_[y * LedMatrix::MATRIX_WIDTH + x] = rgbToColor(r, g, b);
    }

    void drawPixel(const uint8_t red, const uint8_t green, const uint8_t blue)
    {
        drawPixel(cursor_x_, cursor_y_, red, green, blue);
    }

    void getPixel(const uint8_t x, const uint8_t y, uint8_t& r_out, uint8_t& g_out, uint8_t& b_out) const
    {
        r_out = g_out = b_out = 0;

        if (x >= LedMatrix::MATRIX_WIDTH || y >= LedMatrix::MATRIX_HEIGHT) return;

        const uint32_t color = buffer_[y * LedMatrix::MATRIX_WIDTH + x];
        colorToRgb(color, r_out, g_out, b_out);
    }

    void fillRgb(const uint8_t red, const uint8_t green, const uint8_t blue)
    {
        const auto r = static_cast<uint32_t>(red) << 16;
        const auto g = static_cast<uint32_t>(green) << 8;
        const auto b = static_cast<uint32_t>(blue);

        const uint32_t color = r | g | b;
        std::ranges::fill(buffer_, color);
    }

    void drawLine(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t r, const uint8_t g, const uint8_t b)
    {
        const int16_t steep = abs(y1 - y0) > abs(x1 - x0);
        if (steep)
        {
            std::swap(x0, y0);
            std::swap(x1, y1);
        }

        if (x0 > x1)
        {
            std::swap(x0, x1);
            std::swap(y0, y1);
        }

        const int16_t dx = x1 - x0;
        const int16_t dy = abs(y1 - y0);
        int16_t err = dx / 2;
        const int16_t y_step = (y0 < y1) ? 1 : -1;

        for (; x0 <= x1; x0++)
        {
            if (steep)
            {
                drawPixel(y0, x0, r, g, b);
            }
            else
            {
                drawPixel(x0, y0, r, g, b);
            }

            err -= dy;
            if (err < 0)
            {
                y0 += y_step;
                err += dx;
            }
        }
    }

    void drawRect(const uint8_t x, const uint8_t y, const uint8_t w, const uint8_t h,
                  const uint8_t r, const uint8_t g, const uint8_t b)
    {
        drawLine(x, y, x + w - 1, y, r, g, b); // Top
        drawLine(x, y + h - 1, x + w - 1, y + h - 1, r, g, b); // Bottom
        drawLine(x, y, x, y + h - 1, r, g, b); // Left
        drawLine(x + w - 1, y, x + w - 1, y + h - 1, r, g, b); // Right
    }

    void fillRect(const uint8_t x, const uint8_t y, const uint8_t w, const uint8_t h,
                  const uint8_t r, const uint8_t g, const uint8_t b)
    {
        for (uint8_t i = x; i < x + w; i++)
        {
            for (uint8_t j = y; j < y + h; j++)
            {
                drawPixel(i, j, r, g, b);
            }
        }
    }

    void drawCircle(const uint8_t x0, const uint8_t y0, const uint8_t radius,
                    const uint8_t r, const uint8_t g, const uint8_t b)
    {
        int16_t f = 1 - radius;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * radius;
        int16_t x = 0;
        int16_t y = radius;

        drawPixel(x0, y0 + radius, r, g, b);
        drawPixel(x0, y0 - radius, r, g, b);
        drawPixel(x0 + radius, y0, r, g, b);
        drawPixel(x0 - radius, y0, r, g, b);

        while (x < y)
        {
            if (f >= 0)
            {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }

            x++;
            ddF_x += 2;
            f += ddF_x;

            drawPixel(x0 + x, y0 + y, r, g, b);
            drawPixel(x0 - x, y0 + y, r, g, b);
            drawPixel(x0 + x, y0 - y, r, g, b);
            drawPixel(x0 - x, y0 - y, r, g, b);
            drawPixel(x0 + y, y0 + x, r, g, b);
            drawPixel(x0 - y, y0 + x, r, g, b);
            drawPixel(x0 + y, y0 - x, r, g, b);
            drawPixel(x0 - y, y0 - x, r, g, b);
        }
    }

    void fillCircle(const uint8_t x0, const uint8_t y0, const uint8_t radius,
                    const uint8_t r, const uint8_t g, const uint8_t b)
    {
        int16_t f = 1 - radius;
        int16_t ddF_x = 1;
        int16_t ddF_y = -2 * radius;
        int16_t x = 0;
        int16_t y = radius;

        for (int16_t i = -radius; i <= radius; i++)
        {
            drawPixel(x0, y0 + i, r, g, b);
        }

        while (x < y)
        {
            if (f >= 0)
            {
                y--;
                ddF_y += 2;
                f += ddF_y;
            }

            x++;
            ddF_x += 2;
            f += ddF_x;

            for (int16_t i = -y; i <= y; i++)
            {
                drawPixel(x0 + x, y0 + i, r, g, b);
                drawPixel(x0 - x, y0 + i, r, g, b);
            }
            for (int16_t i = -x; i <= x; i++)
            {
                drawPixel(x0 + i, y0 + y, r, g, b);
                drawPixel(x0 + i, y0 - y, r, g, b);
            }
        }
    }

    void drawChar(char c, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t size = 1)
    {
        const uint8_t s = size > 0 ? size : text_size_;

        if (c < ' ' || c > '~')
        {
            c = '?';
        }

        const uint16_t char_offset = (c - ' ') * FONT_WIDTH;

        if (cursor_x_ + FONT_WIDTH * s > LedMatrix::MATRIX_WIDTH)
        {
            if (wrap_text_)
            {
                cursor_x_ = 0;
                cursor_y_ += (FONT_HEIGHT + FONT_SPACING) * s;

                if (cursor_y_ + FONT_HEIGHT * s > LedMatrix::MATRIX_HEIGHT)
                {
                    cursor_y_ = 0;
                }
            }
            else
            {
                return;
            }
        }

        if (cursor_y_ + FONT_HEIGHT * s > LedMatrix::MATRIX_HEIGHT)
        {
            if (wrap_text_)
            {
                cursor_y_ = 0;
            }
            else
            {
                return;
            }
        }

        for (uint8_t col = 0; col < FONT_WIDTH; col++)
        {
            const uint8_t column_data = font5x7[char_offset + col];

            for (uint8_t row = 0; row < FONT_HEIGHT; row++)
            {
                if (column_data & (1 << row))
                {
                    if (s == 1)
                    {
                        drawPixel(cursor_x_ + col, cursor_y_ + row, r, g, b);
                    }
                    else
                    {
                        fillRect(cursor_x_ + (col * s), cursor_y_ + (row * s), s, s, r, g, b);
                    }
                }
            }
        }

        cursor_x_ += (FONT_WIDTH + FONT_SPACING) * s;
    }

    void print(const char* str, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t size = 1)
    {
        while (*str)
        {
            drawChar(*str++, r, g, b, size);
        }
    }

    void println(const char* str, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t size = 0)
    {
        print(str, r, g, b, size);

        cursor_x_ = 0;
        cursor_y_ += (FONT_HEIGHT + FONT_SPACING) * (size > 0 ? size : text_size_);

        if (cursor_y_ + FONT_HEIGHT * (size > 0 ? size : text_size_) > LedMatrix::MATRIX_HEIGHT)
        {
            if (wrap_text_)
            {
                cursor_y_ = 0;
            }
        }
    }
};
