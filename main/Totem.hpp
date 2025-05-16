#pragma once

#include <memory>
#include <mutex>

#include "nlohmann/json.hpp"
#include "PatternBase.hpp"
#include "PatternRegistry.hpp"
#include "util/Http.hpp"
#include "util/ThreadManager.hpp"

class Totem final
{
    static constexpr auto TAG = "Totem";

    static std::atomic<uint8_t> brightness_;
    static ThreadManager render_thread_;
    static std::mutex state_mutex_;
    static std::shared_ptr<PatternBase> active_pattern_;

public:
    Totem() = delete;

    static esp_err_t start()
    {
        ESP_LOGI(TAG, "Starting...");

        render_thread_.start([](const std::atomic<bool>& running)
        {
            while (running.load())
            {
                {
                    std::lock_guard lock(state_mutex_);
                    if (active_pattern_)
                    {
                        active_pattern_->clear();
                        active_pattern_->render();
                        MatrixDriver::loadFromBuffer(active_pattern_->get_buf());
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(16));
            }
        });

        ESP_LOGI(TAG, "Running");
        return ESP_OK;
    }

    static void set_pattern(const std::shared_ptr<PatternBase>& pattern)
    {
        std::lock_guard lock(state_mutex_);
        active_pattern_ = pattern;
    }

    template <typename TPattern, typename... TArgs>
    static void set_pattern(TArgs&&... args)
    {
        std::lock_guard lock(state_mutex_);
        auto state = std::make_shared<TPattern>(std::forward<TArgs>(args)...);
        active_pattern_ = state;
    }

    [[nodiscard]] static esp_err_t set_pattern(const std::string& pattern_name)
    {
        const auto pattern = PatternRegistry::create_pattern(pattern_name);
        if (pattern == nullptr)
        {
            return ESP_ERR_NOT_FOUND;
        }

        set_pattern(pattern);
        return ESP_OK;
    }

    static void set_brightness(const uint8_t brightness)
    {
        std::lock_guard lock(state_mutex_);
        brightness_.store(brightness);
    }
};


std::atomic<uint8_t> Totem::brightness_{255};
ThreadManager Totem::render_thread_("totem_render_thread", 1, 8192, configMAX_PRIORITIES);
std::mutex Totem::state_mutex_;
std::shared_ptr<PatternBase> Totem::active_pattern_;
