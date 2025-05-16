#pragma once

#include <expected>

#include "esp_http_server.h"
#include "esp_timer.h"

namespace util::http
{
    inline std::expected<std::string, esp_err_t> get_req_body(httpd_req_t* req, const std::string& tag)
    {
        const auto free_mem = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
        ESP_LOGI(tag.c_str(), "Received content with length=%d, heap available=%d", req->content_len, free_mem);

        if (req->content_len == 0)
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content was empty");
            return std::unexpected(ESP_FAIL);
        }

        if (req->content_len > free_mem)
        {
            httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Content too big");
            return std::unexpected(ESP_FAIL);
        }

        std::string buffer;
        buffer.resize(req->content_len);

        int total_received = 0;
        while (total_received < req->content_len)
        {
            const auto received = httpd_req_recv(req, &buffer[total_received], req->content_len - total_received);

            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue; // Retry if timeout occurred
            }

            if (received <= 0)
            {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive content");
                return std::unexpected(ESP_FAIL);
            }

            total_received += received;
        }

        return buffer;
    }
}
