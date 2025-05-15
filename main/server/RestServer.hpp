#pragma once

#include "esp_vfs.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include <nlohmann/json.hpp>

class RestServer final
{
    static constexpr auto TAG = "RestServer";

    httpd_handle_t server_handle_ = nullptr;

    esp_err_t provisionSystemInfoEndpoint() const
    {
        constexpr httpd_uri_t system_info_get_uri = {
            .uri = "/system/info",
            .method = HTTP_GET,
            .handler = [](httpd_req_t* req)
            {
                httpd_resp_set_type(req, "application/json");
                nlohmann::json root;
                root["version"] = IDF_VER;
                esp_chip_info_t chip_info;
                esp_chip_info(&chip_info);
                root["cores"] = chip_info.cores;
                const std::string sys_info = root.dump();
                httpd_resp_sendstr(req, sys_info.c_str());
                return ESP_OK;
            },
            .user_ctx = nullptr,
        };
        return httpd_register_uri_handler(server_handle_, &system_info_get_uri);
    }

public:
    ~RestServer()
    {
        if (server_handle_) httpd_stop(server_handle_);
    }

    esp_err_t start()
    {
        ESP_LOGI(TAG, "Starting...");

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.stack_size = 8192;
        config.uri_match_fn = httpd_uri_match_wildcard;

        esp_err_t err = httpd_start(&server_handle_, &config);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to start HTTP server");
            return err;
        }

        err = provisionSystemInfoEndpoint();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to provision system info endpoint");
            return err;
        }

        ESP_LOGI(TAG, "Running");
        return ESP_OK;
    }

    esp_err_t registerEndpoint(const char* uri,
                               const httpd_method_t method,
                               esp_err_t (*handler)(httpd_req_t* req),
                               void* deps = nullptr) const
    {
        const httpd_uri_t endpoint = {
            .uri = uri,
            .method = method,
            .handler = handler,
            .user_ctx = deps,
        };

        const esp_err_t err = httpd_register_uri_handler(server_handle_, &endpoint);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register endpoint: %s", endpoint.uri);
        }
        else
        {
            ESP_LOGI(TAG, "Registered endpoint: %s", endpoint.uri);
        }

        return err;
    }
};
