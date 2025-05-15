#pragma once

#include <complex>
#include <expected>

#include "esp_attr.h"
#include "esp_http_server.h"
#include "esp_timer.h"

namespace util
{
    inline unsigned long IRAM_ATTR millis()
    {
        return static_cast<unsigned long>(esp_timer_get_time() / 1000ULL);
    }

    void fft_recursive_impl(std::complex<float>* x_data, size_t N, std::complex<float>* scratch_data);

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

    inline void fft_recursive_impl(std::complex<float>* x_data, const size_t N, std::complex<float>* scratch_data)
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

    inline std::expected<std::string, esp_err_t> readRequestBody(httpd_req_t* req, const char* tag)
    {
        const auto free_mem = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
        ESP_LOGI(tag, "Received content with length=%d, heap available=%d", req->content_len, free_mem);

        if (req->content_len == 0)
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content was empty");
            return std::unexpected(ESP_FAIL);
        }

        if (req->content_len > free_mem)
        {
            httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Content too big");
            return std::unexpected(ESP_FAIL);
        }

        std::string buffer;
        buffer.resize(req->content_len);

        int total_received = 0;
        while (total_received < req->content_len)
        {
            const auto received = httpd_req_recv(req, &buffer[total_received], req->content_len - total_received);

            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue; // Retry if timeout occurred
            }

            if (received <= 0)
            {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive content");
                return std::unexpected(ESP_FAIL);
            }

            total_received += received;
        }

        return buffer;
    }
}
