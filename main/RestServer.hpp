#pragma once

#include <expected>

#include "esp_vfs.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "totem/Totem.hpp"

#include "endpoints/SystemInfoEndpoint.hpp"
#include "endpoints/LedMatrixRgbEndpoint.hpp"
#include "endpoints/LedMatrixSettingsEndpoint.hpp"
#include "endpoints/LedMatrixGifEndpoint.hpp"

class RestServer
{
    static constexpr auto TAG = "RestServer";

    httpd_handle_t _server_handle = nullptr;

    RestServer() = default;
    ~RestServer() = default;

    void provisionSystemInfoEndpoint() const
    {
        constexpr httpd_uri_t system_info_get_uri = {
            .uri = "/api/v1/system/info",
            .method = HTTP_GET,
            .handler = endpoints::systemInfo,
            .user_ctx = nullptr,
        };
        httpd_register_uri_handler(_server_handle, &system_info_get_uri);
    }

    void provisionRgbEndpoint() const
    {
        constexpr httpd_uri_t system_info_get_uri = {
            .uri = "/api/v1/matrix/rgb",
            .method = HTTP_POST,
            .handler = endpoints::ledMatrixRgb,
            .user_ctx = nullptr,
        };
        httpd_register_uri_handler(_server_handle, &system_info_get_uri);
    }

    void provisionGifEndpoint() const
    {
        constexpr httpd_uri_t system_info_get_uri = {
            .uri = "/api/v1/matrix/gif",
            .method = HTTP_POST,
            .handler = endpoints::ledMatrixGif,
            .user_ctx = nullptr,
        };
        httpd_register_uri_handler(_server_handle, &system_info_get_uri);
    }

    void provisionMatrixSettingsEndpoint() const
    {
        constexpr httpd_uri_t system_info_get_uri = {
            .uri = "/api/v1/matrix/settings",
            .method = HTTP_POST,
            .handler = endpoints::ledMatrixSettings,
            .user_ctx = nullptr,
        };
        httpd_register_uri_handler(_server_handle, &system_info_get_uri);
    }

public:
    static void Start()
    {
        ESP_LOGI(TAG, "Starting...");

        RestServer server;
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.uri_match_fn = httpd_uri_match_wildcard;

        if (httpd_start(&server._server_handle, &config) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start HTTP server");
            abort();
        }

        server.provisionSystemInfoEndpoint();
        server.provisionRgbEndpoint();
        server.provisionGifEndpoint();
        server.provisionMatrixSettingsEndpoint();

        ESP_LOGI(TAG, "Running...");
    }
};
