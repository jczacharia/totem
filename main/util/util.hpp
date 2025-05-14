#pragma once

#include <complex>

#include "esp_attr.h"
#include "esp_timer.h"

namespace util
{
    inline unsigned long IRAM_ATTR millis()
    {
        return static_cast<unsigned long>(esp_timer_get_time() / 1000ULL);
    }

    void fft(std::vector<std::complex<float>>& x)
    {
        const size_t N = x.size();

        // Base case
        if (N <= 1) return;

        // Divide
        std::vector<std::complex<float>> even(N / 2);
        std::vector<std::complex<float>> odd(N / 2);

        for (size_t i = 0; i < N / 2; i++)
        {
            even[i] = x[2 * i];
            odd[i] = x[2 * i + 1];
        }

        // Conquer
        fft(even);
        fft(odd);

        // Combine
        for (size_t k = 0; k < N / 2; k++)
        {
            const float angle = -2.0f * M_PI * static_cast<float>(k) / static_cast<float>(N);
            const std::complex<float> t = std::polar(1.0f, angle) * odd[k];
            x[k] = even[k] + t;
            x[k + N / 2] = even[k] - t;
        }
    }
}
