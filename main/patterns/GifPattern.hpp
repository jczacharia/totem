#pragma once
//
// #include <string>
// #include <atomic>
//
// #include "Totem.hpp"
//
// class GifPattern final : public PatternBase
// {
//     std::string gif_data_;
//     uint8_t* gif_buffer_ = nullptr;
//     std::atomic<uint16_t> frame_idx_{0};
//     uint16_t total_frames_{0};
//
// public:
//     GifPattern(std::string data, const size_t size) : PatternBase("GifPattern")
//     {
//         set_render_tick(pdMS_TO_TICKS(50));
//         gif_data_ = std::move(data);
//         gif_buffer_ = reinterpret_cast<uint8_t*>(gif_data_.data());
//         total_frames_ = size / LedMatrix::MATRIX_BUFFER_SIZE;
//         frame_idx_.store(0);
//     }
//
//     void from_json(const nlohmann::basic_json<>& j) override
//     {
//     }
//
//     esp_err_t override_response(httpd_req_t* req, Totem& totem) override
//     {
//         auto body_or_error = util::http::get_req_body(req, get_name());
//         if (!body_or_error) return body_or_error.error();
//
//         std::string body = std::move(body_or_error.value());
//         const auto size = body.size();
//         char err_str[64];
//
//         if (size < LedMatrix::MATRIX_BUFFER_SIZE)
//         {
//             sprintf(err_str, "GIF buffer must be greater than %d", LedMatrix::MATRIX_BUFFER_SIZE);
//             ESP_LOGE(get_name().c_str(), "%s", err_str);
//             httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_str);
//             return ESP_FAIL;
//         }
//
//         if (size % LedMatrix::MATRIX_BUFFER_SIZE != 0)
//         {
//             sprintf(err_str, "GIF buffer must be divisible than %d", LedMatrix::MATRIX_BUFFER_SIZE);
//             ESP_LOGE(get_name().c_str(), "%s", err_str);
//             httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_str);
//             return ESP_FAIL;
//         }
//
//         totem.set_state<GifPattern>(std::move(body), size);
//
//         httpd_resp_sendstr(req, "GifPattern set successfully");
//         return ESP_OK;
//     }
//
//     void render(MatrixGfx& gfx) override
//     {
//         if (gif_buffer_ != nullptr)
//         {
//             auto buf = reinterpret_cast<uint32_t*>(gif_buffer_);
//             buf = buf + frame_idx_.load() * LedMatrix::MATRIX_SIZE;
//             gfx.set_buf_from_ptr(buf);
//             frame_idx_.store((frame_idx_.load() + 1) % total_frames_);
//         }
//     }
// };
