// #pragma once
//
// #include <cstring>
// #include <thread>
//
// #include "esp_pthread.h"
// #include "LedMatrix.hpp"
//
// class Totem final : public util::Singleton<Totem>
// {
//     static constexpr auto TAG = "Totem";
//     friend class util::Singleton<Totem>;
//     std::mutex mutex_;
//
//     enum class State
//     {
//         RGB,
//         GIF,
//     };
//
//     std::atomic<State> state_{State::RGB};
//
//     std::string gif_data_;
//     uint8_t* gif_buffer_ = nullptr;
//     std::atomic<uint16_t> gif_frame_idx_{0};
//     std::atomic<uint16_t> gif_total_frames_{0};
//
//     std::atomic<uint8_t> red_{0};
//     std::atomic<uint8_t> green_{0};
//     std::atomic<uint8_t> blue_{0};
//
//     static constexpr uint16_t MIN_GIF_SPEED = 50;
//     std::atomic<uint16_t> gif_speed_{MIN_GIF_SPEED};
//
//     std::atomic<uint8_t> brightness_{255};
//
//     std::thread update_thread_;
//     std::atomic<bool> update_thread_running_{true};
//
//     std::thread gif_frame_thread_;
//     std::atomic<bool> gif_frame_thread_running_{true};
//
//     std::thread brightness_thread_;
//     std::atomic<bool> brightness_thread_running_{true};
//
//     void provisionUpdateThread()
//     {
//         ESP_LOGI(TAG, "Provisioning update thread");
//         auto cfg = esp_pthread_get_default_config();
//         cfg.thread_name = "totem_update_thread";
//         cfg.pin_to_core = 1;
//         cfg.stack_size = 4096;
//         cfg.prio = configMAX_PRIORITIES;
//         esp_pthread_set_cfg(&cfg);
//         update_thread_ = std::thread([this]
//         {
//             const auto sleep_time = std::chrono::milliseconds(MIN_GIF_SPEED);
//
//             while (update_thread_running_)
//             {
//                 {
//                     std::lock_guard lock(mutex_);
//
//                     switch (state_.load())
//                     {
//                     case State::GIF:
//                         if (gif_buffer_ != nullptr)
//                         {
//                             auto buf = reinterpret_cast<volatile uint32_t*>(gif_buffer_);
//                             buf = buf + gif_frame_idx_.load() * LedMatrix::MATRIX_SIZE;
//                             LedMatrix::getInstance().loadFromBuffer(buf);
//                         }
//                         else
//                         {
//                             state_.store(State::RGB);
//                         }
//                         break;
//
//                     case State::RGB:
//                     default:
//                         LedMatrix::getInstance().fillRgb(red_.load(), green_.load(), blue_.load());
//                         break;
//                     }
//                 }
//
//                 std::this_thread::sleep_for(sleep_time);
//             }
//         });
//         update_thread_.detach();
//     }
//
//     void provisionGifFrameThread()
//     {
//         ESP_LOGI(TAG, "Provisioning gif frame thread");
//         auto cfg = esp_pthread_get_default_config();
//         cfg.thread_name = "totem_gif_frame_thread";
//         cfg.pin_to_core = 1;
//         cfg.stack_size = 1024;
//         cfg.prio = configMAX_PRIORITIES - 1;
//         esp_pthread_set_cfg(&cfg);
//         gif_frame_thread_ = std::thread([this]
//         {
//             while (gif_frame_thread_running_)
//             {
//                 if (state_.load() == State::GIF)
//                 {
//                     if (const uint16_t total_frames = gif_total_frames_.load(); total_frames > 0)
//                     {
//                         gif_frame_idx_.store((gif_frame_idx_.load() + 1) % total_frames);
//                     }
//                 }
//
//                 const auto speed = std::max(gif_speed_.load(), MIN_GIF_SPEED);
//                 std::this_thread::sleep_for(std::chrono::milliseconds(speed));
//             }
//         });
//         gif_frame_thread_.detach();
//     }
//
//     void provisionBrightnessThread()
//     {
//         ESP_LOGI(TAG, "Provisioning brightness thread");
//         auto cfg = esp_pthread_get_default_config();
//         cfg.thread_name = "totem_brightness_thread";
//         cfg.pin_to_core = 1;
//         cfg.stack_size = 1024;
//         cfg.prio = configMAX_PRIORITIES - 2;
//         esp_pthread_set_cfg(&cfg);
//         brightness_thread_ = std::thread([this]
//         {
//             const auto sleep_time = std::chrono::milliseconds(1000);
//             std::atomic<uint8_t> brightness_cache = 0;
//
//             while (brightness_thread_running_)
//             {
//                 const auto b_val = brightness_.load();
//                 if (const auto b_cache = brightness_cache.load(); b_val != b_cache)
//                 {
//                     std::lock_guard lock(mutex_);
//                     LedMatrix::getInstance().setBrightness(b_val);
//                     brightness_cache.store(b_val);
//                 }
//
//                 std::this_thread::sleep_for(sleep_time);
//             }
//         });
//         brightness_thread_.detach();
//     }
//
//     Totem()
//     {
//         ESP_LOGI(TAG, "Initializing...");
//         provisionUpdateThread();
//         provisionGifFrameThread();
//         provisionBrightnessThread();
//         ESP_LOGI(TAG, "Running...");
//     }
//
//     ~Totem() override
//     {
//         ESP_LOGI(TAG, "Destroying...");
//
//         update_thread_running_ = false;
//         if (update_thread_.joinable()) update_thread_.join();
//
//         gif_frame_thread_running_ = false;
//         if (gif_frame_thread_.joinable()) gif_frame_thread_.join();
//
//         brightness_thread_running_ = false;
//         if (brightness_thread_.joinable()) brightness_thread_.join();
//
//         ESP_LOGI(TAG, "Destroyed");
//     }
//
// public:
//     void setRgbMode(const uint8_t r, const uint8_t g, const uint8_t b)
//     {
//         {
//             std::lock_guard lock(mutex_);
//             red_.store(r);
//             green_.store(g);
//             blue_.store(b);
//             state_.store(State::RGB);
//         }
//         ESP_LOGI(TAG, "Set to solid color: R=%d, G=%d, B=%d", r, g, b);
//     }
//
//     void setSettings(const uint8_t brightness, const uint16_t speed)
//     {
//         brightness_ = brightness;
//         gif_speed_ = speed;
//         ESP_LOGI(TAG, "Brightness set to %d and speed set to %d", brightness, speed);
//     }
//
//     void setGifMode(std::string buf, const size_t size)
//     {
//         const auto frames = size / LedMatrix::MATRIX_BUFFER_SIZE;
//         {
//             std::lock_guard lock(mutex_);
//             gif_data_ = std::move(buf);
//             gif_buffer_ = reinterpret_cast<uint8_t*>(&gif_data_[0]);
//             gif_total_frames_.store(frames);
//             state_.store(State::GIF);
//         }
//         ESP_LOGI(TAG, "Set to GIF mode with frames=%d", frames);
//     }
// };
