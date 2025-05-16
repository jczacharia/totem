#pragma once

#include "../PatternBase.hpp"
#include "Totem.hpp"
#include <random>
#include <vector>

class FirePattern final : public PatternBase
{
private:
    // Fire effect parameters
    uint8_t cooling_;
    uint8_t sparking_;
    std::vector<uint8_t> heat_;

    // Random number generator
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_int_distribution<> spark_dist_;
    std::uniform_int_distribution<> cooling_dist_;

public:
    explicit FirePattern(
        const uint8_t cooling = 55,
        const uint8_t sparking = 120)
        : PatternBase("FirePattern"),
          cooling_(cooling),
          sparking_(sparking),
          heat_(MatrixDriver::WIDTH * MatrixDriver::HEIGHT, 0),
          gen_(rd_()),
          spark_dist_(0, 255),
          cooling_dist_(0, cooling_)
    {
        set_render_tick(pdMS_TO_TICKS(30)); // 30ms refresh rate for animation
    }

    void from_json(const nlohmann::basic_json<>& j) override
    {
        cooling_ = j.value("cooling", 55);
        sparking_ = j.value("sparking", 120);
        cooling_dist_ = std::uniform_int_distribution<>(0, cooling_);
    }

    void render() override
    {
        // Step 1. Cool down every cell a little
        for (int i = 0; i < heat_.size(); i++)
        {
            int cooldown = cooling_dist_(gen_);

            if (cooldown > heat_[i])
            {
                heat_[i] = 0;
            }
            else
            {
                heat_[i] = heat_[i] - cooldown;
            }
        }

        // Step 2. Heat rises - move heat from each cell to one above
        for (int y = MatrixDriver::HEIGHT - 1; y >= 2; y--)
        {
            for (int x = 0; x < MatrixDriver::WIDTH; x++)
            {
                int idx = y * MatrixDriver::WIDTH + x;
                int below_idx = (y - 1) * MatrixDriver::WIDTH + x;
                int below2_idx = (y - 2) * MatrixDriver::WIDTH + x;

                // Mix the pixels below to create a flickering effect
                heat_[idx] = (heat_[below_idx] + heat_[below2_idx] * 2) / 3;
            }
        }

        // Step 3. Randomly ignite new 'sparks' at the bottom
        for (int x = 0; x < MatrixDriver::WIDTH; x++)
        {
            if (spark_dist_(gen_) < sparking_)
            {
                int y = 1;
                int idx = y * MatrixDriver::WIDTH + x;
                heat_[idx] = std::min<int>(heat_[idx] + spark_dist_(gen_) / 2, 255);
            }
        }

        // Step 4. Convert heat to LED colors and draw
        for (int y = 0; y < MatrixDriver::HEIGHT; y++)
        {
            for (int x = 0; x < MatrixDriver::WIDTH; x++)
            {
                int idx = y * MatrixDriver::WIDTH + x;
                uint8_t heat_val = heat_[idx];

                // Calculate RGB color based on heat value (HSV palette)
                // Hotter = more yellow/white, cooler = more red/black
                uint8_t r = heat_val;
                uint8_t g = heat_val > 128 ? (heat_val - 128) * 2 : 0;
                uint8_t b = heat_val > 192 ? (heat_val - 192) * 4 : 0;

                draw_pixel_rgb(x, y, r, g, b);
            }
        }
    }
};
