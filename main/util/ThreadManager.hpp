#pragma once

#include <thread>
#include <functional>
#include <atomic>
#include <string>

#include "esp_pthread.h"
#include "esp_log.h"

class ThreadManager
{
    static constexpr auto TAG = "ThreadManager";

    std::thread thread_;
    std::atomic<bool> running_{true};
    std::mutex shutdown_mutex_;
    std::string name_;

    int core_;
    size_t stack_size_;
    size_t priority_;

public:
    ThreadManager(std::string name, const int core, const size_t stack_size, const size_t priority)
        : name_(std::move(name)), core_(core), stack_size_(stack_size), priority_(priority)
    {
    }

    ~ThreadManager()
    {
        stop();
    }

    void start(std::function<void(std::atomic<bool>&)> thread_func)
    {
        ESP_LOGI(TAG, "Starting thread: %s", name_.c_str());
        auto cfg = esp_pthread_get_default_config();
        cfg.thread_name = name_.c_str();
        cfg.pin_to_core = core_;
        cfg.stack_size = stack_size_;
        cfg.prio = priority_;
        esp_pthread_set_cfg(&cfg);
        thread_ = std::thread([this, func = std::move(thread_func)] { func(running_); });
    }

    void stop()
    {
        ESP_LOGI(TAG, "Stopping thread: %s", name_.c_str());
        running_.store(false);

        if (thread_.joinable())
        {
            thread_.join();
            ESP_LOGI(TAG, "Thread stopped: %s", name_.c_str());
        }
        else
        {
            ESP_LOGW(TAG, "Thread %s is not joinable", name_.c_str());
        }
    }

    [[nodiscard]] bool isRunning() const
    {
        return running_.load();
    }
};
