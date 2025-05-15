#pragma once

#include <string>
#include <atomic>

#include "TotemState.hpp"

class GifState final : public TotemState
{
    static constexpr auto TAG = "GifState";

    std::string gif_data_;
    uint8_t* gif_buffer_ = nullptr;
    std::atomic<uint16_t> frame_idx_{0};
    uint16_t total_frames_{0};

public:
    GifState(std::string data, const size_t size)
    {
        setRenderTick(pdMS_TO_TICKS(50));
        setGifData(std::move(data), size);
    }

    void render(MatrixGfx& gfx) override
    {
        if (gif_buffer_ != nullptr)
        {
            auto buf = reinterpret_cast<uint32_t*>(gif_buffer_);
            buf = buf + frame_idx_.load() * LedMatrix::MATRIX_SIZE;
            gfx.setBufferFromPtr(buf);
            frame_idx_.store((frame_idx_.load() + 1) % total_frames_);
        }
    }

    void setGifData(std::string data, const size_t size)
    {
        gif_data_ = std::move(data);
        gif_buffer_ = reinterpret_cast<uint8_t*>(gif_data_.data());
        total_frames_ = size / LedMatrix::MATRIX_BUFFER_SIZE;
        frame_idx_.store(0);
    }

    static esp_err_t endpoint(httpd_req_t* req)
    {
        auto body_or_error = util::readRequestBody(req, TAG);
        if (!body_or_error) return body_or_error.error();

        std::string body = std::move(body_or_error.value());
        const auto size = body.size();
        char err_str[64];

        if (size < LedMatrix::MATRIX_BUFFER_SIZE)
        {
            sprintf(err_str, "GIF buffer must be greater than %d", LedMatrix::MATRIX_BUFFER_SIZE);
            ESP_LOGE(TAG, "%s", err_str);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_str);
            return ESP_FAIL;
        }

        if (size % LedMatrix::MATRIX_BUFFER_SIZE != 0)
        {
            sprintf(err_str, "GIF buffer must be divisible than %d", LedMatrix::MATRIX_BUFFER_SIZE);
            ESP_LOGE(TAG, "%s", err_str);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_str);
            return ESP_FAIL;
        }

        Totem::setState<GifState>(std::move(body), size);

        httpd_resp_sendstr(req, "GifState set successfully");
        return ESP_OK;
    }
};
