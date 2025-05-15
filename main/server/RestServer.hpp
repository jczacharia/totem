#pragma once

#include <expected>

#include "cJSON.h"
#include "esp_vfs.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_chip_info.h"

#include "util/Singleton.hpp"

class RestServer final : public util::Singleton<RestServer>
{
    friend class util::Singleton<RestServer>;
    static constexpr auto TAG = "RestServer";

    httpd_handle_t server_handle_ = nullptr;

    RestServer()
    {
        ESP_LOGI(TAG, "Starting...");

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.stack_size = 8192;
        config.uri_match_fn = httpd_uri_match_wildcard;

        if (httpd_start(&server_handle_, &config) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start HTTP server");
            abort();
        }

        provisionSystemInfoEndpoint();

        ESP_LOGI(TAG, "Running...");
    }

    ~RestServer() override
    {
        if (server_handle_) httpd_stop(server_handle_);
    }

    void provisionSystemInfoEndpoint() const
    {
        constexpr httpd_uri_t system_info_get_uri = {
            .uri = "/system/info",
            .method = HTTP_GET,
            .handler = [](httpd_req_t* req)
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
        httpd_register_uri_handler(server_handle_, &system_info_get_uri);
    }

public:
    void registerEndpoint(const char* uri, const httpd_method_t method, esp_err_t (*handler)(httpd_req_t* req)) const
    {
        const httpd_uri_t endpoint = {
            .uri = uri,
            .method = method,
            .handler = handler,
            .user_ctx = nullptr,
        };

        if (httpd_register_uri_handler(server_handle_, &endpoint) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register endpoint: %s", endpoint.uri);
        }
        else
        {
            ESP_LOGI(TAG, "Registered endpoint: %s", endpoint.uri);
        }
    }
};
