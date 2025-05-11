#pragma once

#include "esp_attr.h"
#include "esp_timer.h"

namespace Util
{
    inline unsigned long IRAM_ATTR millis()
    {
        return static_cast<unsigned long>(esp_timer_get_time() / 1000ULL);
    }

    template <size_t N>
    static constexpr std::array<uint8_t, N> createGammaTable()
    {
        std::array<uint8_t, N> table{};

        for (size_t i = 0; i < N; i++)
        {
            const double normalized = static_cast<double>(i) / 255.0;
            double result = 0.0;
            if (normalized <= 0.0)
            {
                result = 0.0;
            }
            else if (normalized >= 1.0)
            {
                result = 1.0;
            }
            else
            {
                result = normalized * normalized * (normalized * 0.2 + 0.8);
            }

            table[i] = static_cast<uint8_t>(result * 255.0 + 0.5);
        }

        return table;
    }
}
