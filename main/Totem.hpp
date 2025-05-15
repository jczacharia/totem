#pragma once

#include <memory>
#include <mutex>

#include "MatrixGfx.hpp"
#include "nlohmann/json.hpp"
#include "patterns/PatternBase.hpp"
#include "util/Http.hpp"
#include "util/ThreadManager.hpp"

class Totem final
{
    static constexpr auto TAG = "Totem";

    MatrixGfx& gfx_;
    std::mutex state_mutex_;
    std::atomic<uint8_t> brightness_{255};
    std::shared_ptr<PatternBase> current_state_;

    ThreadManager render_thread_{"totem_render_thread", 1, 8192, configMAX_PRIORITIES};

public:
    explicit Totem(MatrixGfx& gfx) : gfx_(gfx)
    {
    }

    esp_err_t start()
    {
        ESP_LOGI(TAG, "Starting...");

        render_thread_.start([this](const std::atomic<bool>& running)
        {
            TickType_t tick = PatternBase::DEFAULT_RENDER_TICK;
            while (running.load())
            {
                {
                    std::lock_guard lock(state_mutex_);
                    if (current_state_)
                    {
                        gfx_.clear();
                        current_state_->render(gfx_);
                        gfx_.flush(brightness_.load());
                        tick = current_state_->getRenderTick();
                    }
                }
                vTaskDelay(tick);
            }
        });

        ESP_LOGI(TAG, "Running");
        return ESP_OK;
    }

    template <typename TState, typename... TArgs>
    void set_state(TArgs&&... args)
    {
        std::lock_guard lock(state_mutex_);
        auto state = std::make_shared<TState>(std::forward<TArgs>(args)...);
        current_state_ = state;
    }

    static esp_err_t brightness_endpoint(httpd_req_t* req)
    {
        auto body_or_error = util::http::get_req_body(req, TAG);
        if (!body_or_error) return body_or_error.error();

        const auto j = nlohmann::json::parse(body_or_error.value());

        const auto totem = static_cast<Totem*>(req->user_ctx);
        std::lock_guard lock(totem->state_mutex_);
        totem->brightness_.store(j.value("brightness", 255));

        httpd_resp_sendstr(req, "Brightness set successfully");
        return ESP_OK;
    }
};
