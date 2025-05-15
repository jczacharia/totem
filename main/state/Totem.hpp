#pragma once

#include <memory>
#include <mutex>

#include "util/ThreadManager.hpp"
#include "util/Singleton.hpp"

#include "matrix/MatrixGfx.hpp"

#include "TotemState.hpp"
#include "StateLayer.hpp"

class Totem final : public util::Singleton<Totem>
{
    static constexpr auto TAG = "Totem";
    friend class util::Singleton<Totem>;

    std::mutex state_mutex_;
    std::shared_ptr<TotemState> current_state_;

    ThreadManager render_thread_{"totem_render_thread", 1, 8192, configMAX_PRIORITIES};

    Totem()
    {
        ESP_LOGI(TAG, "Initializing...");

        render_thread_.start([this](const std::atomic<bool>& running)
        {
            TickType_t tick = TotemState::DEFAULT_RENDER_TICK;
            while (running.load())
            {
                {
                    std::lock_guard lock(state_mutex_);
                    if (current_state_)
                    {
                        MatrixGfx& gfx = MatrixGfx::getInstance();
                        gfx.clear();
                        current_state_->render(gfx);
                        gfx.flush();
                        tick = current_state_->getRenderTick();
                    }
                }
                vTaskDelay(tick);
            }
        });

        ESP_LOGI(TAG, "Running...");
    }

public:
    template <typename TState, typename... TArgs>
    static void setState(TArgs&&... args)
    {
        Totem& instance = getInstance();
        std::lock_guard lock(instance.state_mutex_);
        auto state = std::make_shared<TState>(std::forward<TArgs>(args)...);
        instance.current_state_ = state;
    }
};
