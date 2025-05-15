#pragma once

#include <complex>

namespace util
{
    static void fft_recursive_impl(std::complex<float>* x_data, size_t N, std::complex<float>* scratch_data);

    inline void fft(std::vector<std::complex<float>>& x)
    {
        const size_t N = x.size();
        if (N == 0) return;

        if ((N > 1) && (N & (N - 1)) != 0)
        {
            ESP_LOGE("FFT", "FFT size %zu is not a power of 2. Aborting FFT.", N);
            return;
        }
        if (N <= 1) return;

        std::vector<std::complex<float>> scratch(N);
        fft_recursive_impl(x.data(), N, scratch.data());
    }

    static void fft_recursive_impl(std::complex<float>* x_data, const size_t N, std::complex<float>* scratch_data)
    {
        if (N <= 1) return;

        std::complex<float>* even_part_in_scratch = scratch_data;
        std::complex<float>* odd_part_in_scratch = scratch_data + N / 2;

        for (size_t i = 0; i < N / 2; i++)
        {
            even_part_in_scratch[i] = x_data[2 * i];
            odd_part_in_scratch[i] = x_data[2 * i + 1];
        }

        fft_recursive_impl(even_part_in_scratch, N / 2, x_data);
        fft_recursive_impl(odd_part_in_scratch, N / 2, x_data + N / 2);

        for (size_t k = 0; k < N / 2; k++)
        {
            const float angle = -2.0f * M_PI * static_cast<float>(k) / static_cast<float>(N);
            const std::complex<float> t = std::polar(1.0f, angle) * odd_part_in_scratch[k];

            x_data[k] = even_part_in_scratch[k] + t;
            x_data[k + N / 2] = even_part_in_scratch[k] - t;
        }
    }
}
