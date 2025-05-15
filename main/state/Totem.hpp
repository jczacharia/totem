#pragma once

#include <memory>
#include <mutex>

#include "util/ThreadManager.hpp"
#include "matrix/MatrixGfx.hpp"
#include "TotemState.hpp"

class Totem final
{
    static constexpr auto TAG = "Totem";

    MatrixGfx& gfx_;
    std::mutex state_mutex_;
    std::shared_ptr<TotemState> current_state_;

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
            TickType_t tick = TotemState::DEFAULT_RENDER_TICK;
            while (running.load())
            {
                {
                    std::lock_guard lock(state_mutex_);
                    if (current_state_)
                    {
                        gfx_.clear();
                        current_state_->render(gfx_);
                        gfx_.flush();
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
    void setState(TArgs&&... args)
    {
        std::lock_guard lock(state_mutex_);
        auto state = std::make_shared<TState>(std::forward<TArgs>(args)...);
        current_state_ = state;
    }
};
