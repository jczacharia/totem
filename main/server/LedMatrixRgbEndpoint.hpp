// #pragma once
//
// #include "cJSON.h"
// #include "esp_err.h"
// #include "EndpointHelpers.hpp"
//
// namespace endpoints
// {
//     inline esp_err_t ledMatrixRgb(httpd_req_t* req)
//     {
//         static constexpr auto TAG = "LedMatrixRgbEndpoint";
//
//         auto body_or_error = readRequestBody(req, TAG);
//         if (!body_or_error) return body_or_error.error();
//
//         cJSON* root = cJSON_Parse(body_or_error.value().c_str());
//         const uint8_t red = cJSON_GetObjectItem(root, "red")->valueint;
//         const uint8_t green = cJSON_GetObjectItem(root, "green")->valueint;
//         const uint8_t blue = cJSON_GetObjectItem(root, "blue")->valueint;
//         cJSON_Delete(root);
//
//         Totem::getInstance().setSolidColorMode(red, green, blue);
//
//         httpd_resp_sendstr(req, "RGB changed successfully");
//         return ESP_OK;
//     }
// }
