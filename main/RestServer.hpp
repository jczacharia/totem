#pragma once

#include "esp_vfs.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include <nlohmann/json.hpp>

#include "PatternRegistry.hpp"

class RestServer final
{
    static constexpr auto TAG = "RestServer";
    static httpd_handle_t server_handle_;

public:
    RestServer() = delete;

    static void stop()
    {
        if (server_handle_) httpd_stop(server_handle_);
    }

    static esp_err_t start()
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

        err = reg_sys_info_endpoint();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register system info endpoint");
            return err;
        }

        err = reg_brightness_endpoint();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register brightness endpoint");
            return err;
        }

        err = reg_pattern_endpoint();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to register pattern endpoint");
            return err;
        }

        ESP_LOGI(TAG, "Running");
        return ESP_OK;
    }

private:
    [[nodiscard]] static esp_err_t reg_sys_info_endpoint()
    {
        constexpr httpd_uri_t system_info_get_uri = {
            .uri = "/api/system/info",
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

    [[nodiscard]] static esp_err_t reg_brightness_endpoint()
    {
        constexpr httpd_uri_t system_info_get_uri = {
            .uri = "/api/brightness",
            .method = HTTP_POST,
            .handler = [](httpd_req_t* req)
            {
                auto body_or_error = util::http::get_req_body(req, TAG);
                if (!body_or_error) return body_or_error.error();
                const auto j = nlohmann::json::parse(body_or_error.value());
                Totem::set_brightness(j.value("brightness", 255));
                httpd_resp_sendstr(req, "Brightness set successfully");
                return ESP_OK;
            },
            .user_ctx = nullptr,
        };
        return httpd_register_uri_handler(server_handle_, &system_info_get_uri);
    }

    [[nodiscard]] static esp_err_t reg_pattern_endpoint()
    {
        // GET endpoint to list available patterns
        constexpr httpd_uri_t pattern_list_uri = {
            .uri = "/api/patterns",
            .method = HTTP_GET,
            .handler = [](httpd_req_t* req)
            {
                httpd_resp_set_type(req, "application/json");
                nlohmann::json response;
                response["patterns"] = PatternRegistry::get_pattern_names();
                const std::string response_str = response.dump();
                httpd_resp_sendstr(req, response_str.c_str());
                return ESP_OK;
            },
            .user_ctx = nullptr,
        };

        if (const esp_err_t err = httpd_register_uri_handler(server_handle_, &pattern_list_uri); err != ESP_OK)
        {
            return err;
        }

        // POST endpoint to set a pattern
        constexpr httpd_uri_t pattern_post_uri = {
            .uri = "/api/pattern",
            .method = HTTP_POST,
            .handler = [](httpd_req_t* req)
            {
                // First, get the request body
                auto body_or_error = util::http::get_req_body(req, TAG);
                if (!body_or_error) return body_or_error.error();

                try
                {
                    auto body = body_or_error.value();
                    const auto json = nlohmann::json::parse(body);

                    // Get the pattern name
                    if (!json.contains("name"))
                    {
                        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Pattern name required");
                        return ESP_FAIL;
                    }

                    const std::string pattern_name = json["name"];

                    const auto pattern = PatternRegistry::create_pattern(pattern_name);
                    if (pattern == nullptr)
                    {
                        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Pattern not found");
                        return ESP_FAIL;
                    }

                    pattern->from_json(json);
                    Totem::set_pattern(pattern);
                    httpd_resp_sendstr(req, pattern->get_name().c_str());
                    return ESP_OK;
                }
                catch (const nlohmann::json::exception& e)
                {
                    ESP_LOGE(TAG, "JSON parsing error: %s", e.what());
                    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
                    return ESP_FAIL;
                }
                catch (const std::exception& e)
                {
                    ESP_LOGE(TAG, "Error: %s", e.what());
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal error");
                    return ESP_FAIL;
                }
            },
            .user_ctx = nullptr,
        };

        return httpd_register_uri_handler(server_handle_, &pattern_post_uri);
    }
};
