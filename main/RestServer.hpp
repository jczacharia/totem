#pragma once

#include <expected>

#include "cJSON.h"
#include "esp_vfs.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "LedMatrix.hpp"
#include "esp_chip_info.h"
#include "Totem.hpp"

class RestServer
{
    static constexpr auto TAG = "RestServer";

    Totem& _totem;
    httpd_handle_t _server_handle = nullptr;

    explicit RestServer(Totem& totem): _totem(totem)
    {
    }

    void provisionSystemInfoEndpoint() const
    {
        constexpr httpd_uri_t system_info_get_uri = {
            .uri = "/api/v1/system/info",
            .method = HTTP_GET,
            .handler = [](httpd_req_t* req) -> esp_err_t
            {
                httpd_resp_set_type(req, "application/json");
                cJSON* root = cJSON_CreateObject();
                esp_chip_info_t chip_info;
                esp_chip_info(&chip_info);
                cJSON_AddStringToObject(root, "version", IDF_VER);
                cJSON_AddNumberToObject(root, "cores", chip_info.cores);
                char* sys_info = cJSON_Print(root);
                httpd_resp_sendstr(req, sys_info);
                free(sys_info);
                cJSON_Delete(root);
                return ESP_OK;
            },
            .user_ctx = nullptr,
        };
        httpd_register_uri_handler(_server_handle, &system_info_get_uri);
    }

    void provisionRgbEndpoint() const
    {
        const httpd_uri_t system_info_get_uri = {
            .uri = "/api/v1/matrix/rgb",
            .method = HTTP_POST,
            .handler = [](httpd_req_t* req) -> esp_err_t
            {
                auto body_or_error = readRequestBody(req);
                if (!body_or_error) return body_or_error.error();

                cJSON* root = cJSON_Parse(body_or_error.value().c_str());
                const uint8_t red = cJSON_GetObjectItem(root, "red")->valueint;
                const uint8_t green = cJSON_GetObjectItem(root, "green")->valueint;
                const uint8_t blue = cJSON_GetObjectItem(root, "blue")->valueint;
                cJSON_Delete(root);

                const auto totem = static_cast<Totem*>(req->user_ctx);
                totem->setRgbMode(red, green, blue);

                httpd_resp_sendstr(req, "RGB changed successfully");
                return ESP_OK;
            },
            .user_ctx = &_totem,
        };
        httpd_register_uri_handler(_server_handle, &system_info_get_uri);
    }

    void provisionGifEndpoint() const
    {
        const httpd_uri_t system_info_get_uri = {
            .uri = "/api/v1/matrix/gif",
            .method = HTTP_POST,
            .handler = [](httpd_req_t* req) -> esp_err_t
            {
                auto body_or_error = readRequestBody(req);
                if (!body_or_error) return body_or_error.error();

                std::string body = std::move(body_or_error.value());
                const auto size = body.size();

                if (size < LedMatrix::MATRIX_BUFFER_SIZE)
                {
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "GIF buffer must be greater than 16384");
                    return ESP_FAIL;
                }

                if (size % LedMatrix::MATRIX_BUFFER_SIZE != 0)
                {
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "GIF buffer must be divisible by 16384");
                    return ESP_FAIL;
                }

                const auto totem = static_cast<Totem*>(req->user_ctx);
                totem->setGifMode(std::move(body), size);

                httpd_resp_sendstr(req, "RGB changed successfully");
                return ESP_OK;
            },
            .user_ctx = &_totem,
        };
        httpd_register_uri_handler(_server_handle, &system_info_get_uri);
    }

    void provisionMatrixSettingsEndpoint() const
    {
        const httpd_uri_t system_info_get_uri = {
            .uri = "/api/v1/matrix/settings",
            .method = HTTP_POST,
            .handler = [](httpd_req_t* req) -> esp_err_t
            {
                auto body_or_error = readRequestBody(req);
                if (!body_or_error) return body_or_error.error();

                cJSON* root = cJSON_Parse(body_or_error.value().c_str());
                const uint8_t brightness = cJSON_GetObjectItem(root, "brightness")->valueint;
                const uint16_t speed = cJSON_GetObjectItem(root, "speed")->valueint;
                cJSON_Delete(root);

                const auto totem = static_cast<Totem*>(req->user_ctx);
                totem->setSettings(brightness, speed);

                httpd_resp_sendstr(req, "LED Matrix settings updated");
                return ESP_OK;
            },
            .user_ctx = &_totem,
        };
        httpd_register_uri_handler(_server_handle, &system_info_get_uri);
    }

public:
    static void Start(Totem& totem)
    {
        ESP_LOGI(TAG, "Starting server");

        RestServer server(totem);
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.uri_match_fn = httpd_uri_match_wildcard;

        if (httpd_start(&server._server_handle, &config) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start HTTP server");
        }

        server.provisionSystemInfoEndpoint();
        server.provisionRgbEndpoint();
        server.provisionGifEndpoint();
        server.provisionMatrixSettingsEndpoint();

        ESP_LOGI(TAG, "Server running");
    }

private:
    static std::expected<std::string, esp_err_t> readRequestBody(httpd_req_t* req)
    {
        auto f = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
        ESP_LOGI(TAG, "Received content with length=%d, heap available=%d", req->content_len, f);

        if (req->content_len == 0)
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content was empty");
            return std::unexpected(ESP_FAIL);
        }

        if (req->content_len > f)
        {
            httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "GIF too big");
            return std::unexpected(ESP_FAIL);
        }

        std::string buffer;
        buffer.resize(req->content_len);

        int total_received = 0;
        while (total_received < req->content_len)
        {
            const auto received = httpd_req_recv(req, &buffer[total_received], req->content_len - total_received);
            if (received <= 0)
            {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
                return std::unexpected(ESP_FAIL);
            }
            total_received += received;
        }

        return buffer;
    }
};
