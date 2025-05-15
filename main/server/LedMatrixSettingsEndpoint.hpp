// #pragma once
//
// #include "cJSON.h"
// #include "esp_err.h"
//
// namespace endpoints
// {
//     inline esp_err_t ledMatrixSettings(httpd_req_t* req)
//     {
//         static constexpr auto TAG = "LedMatrixSettingsEndpoint";
//
//         auto body_or_error = readRequestBody(req, TAG);
//         if (!body_or_error) return body_or_error.error();
//
//         cJSON* root = cJSON_Parse(body_or_error.value().c_str());
//         const uint8_t brightness = cJSON_GetObjectItem(root, "brightness")->valueint;
//         const uint16_t speed = cJSON_GetObjectItem(root, "speed")->valueint;
//         cJSON_Delete(root);
//
//         Totem::getInstance().setSettings(brightness, speed);
//
//         httpd_resp_sendstr(req, "LED Matrix settings updated");
//         return ESP_OK;
//     }
// }
