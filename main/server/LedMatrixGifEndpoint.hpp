// #pragma once
//
// #include "esp_err.h"
//
// namespace endpoints
// {
//     inline esp_err_t ledMatrixGif(httpd_req_t* req)
//     {
//         static constexpr auto TAG = "LedMatrixGifEndpoint";
//
//         auto body_or_error = readRequestBody(req, TAG);
//         if (!body_or_error) return body_or_error.error();
//
//         std::string body = std::move(body_or_error.value());
//         const auto size = body.size();
//         char err_str[64];
//
//         if (size < LedMatrix::MATRIX_BUFFER_SIZE)
//         {
//             sprintf(err_str, "GIF buffer must be greater than %d", LedMatrix::MATRIX_BUFFER_SIZE);
//             httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_str);
//             return ESP_FAIL;
//         }
//
//         if (size % LedMatrix::MATRIX_BUFFER_SIZE != 0)
//         {
//             sprintf(err_str, "GIF buffer must be divisible than %d", LedMatrix::MATRIX_BUFFER_SIZE);
//             httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_str);
//             return ESP_FAIL;
//         }
//
//         Totem::getInstance().setGifMode(std::move(body), size);
//
//         httpd_resp_sendstr(req, "GIF uploaded successfully");
//         return ESP_OK;
//     }
// }
