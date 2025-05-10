#pragma once

#include <thread>
#include <esp_pthread.h>

#include "LedMatrix.hpp"

class Totem
{
    static constexpr auto TAG = "Totem";

    enum class State
    {
        RGB,
        GIF,
    };

    State _state = State::RGB;

    const LedMatrix& _led_matrix;
    SemaphoreHandle_t _mutex = nullptr;

    std::string _gif_data;
    uint8_t* _buffer = nullptr;

    std::atomic<uint8_t> _red = 0;
    std::atomic<uint8_t> _green = 0;
    std::atomic<uint8_t> _blue = 0;

    uint16_t _gif_frame_idx = 0;
    uint16_t _gif_total_frames = 0;

    static constexpr uint16_t MIN_GIF_SPEED = 10;
    std::atomic<uint16_t> _gif_speed{50};

    std::atomic<uint8_t> _brightness{255};

    std::thread _update_thread;
    std::atomic<bool> _update_thread_running{true};

    void provisionUpdateThread()
    {
        ESP_LOGI(TAG, "Provisioning update thread");
        auto cfg = esp_pthread_get_default_config();
        cfg.thread_name = "totem_update_thread";
        cfg.pin_to_core = 1;
        cfg.stack_size = 4096;
        cfg.prio = configMAX_PRIORITIES;
        esp_pthread_set_cfg(&cfg);

        ESP_LOGI(TAG, "Starting update thread");
        const auto sleep_time = std::chrono::milliseconds(MIN_GIF_SPEED);
        _update_thread = std::thread([this, sleep_time]
        {
            while (_update_thread_running)
            {
                if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
                {
                    switch (_state)
                    {
                    case State::GIF:
                        if (_buffer != nullptr)
                        {
                            const auto buf = reinterpret_cast<uint32_t*>(_buffer);
                            _led_matrix.loadFromBuffer(buf + _gif_frame_idx * LedMatrix::MATRIX_SIZE);
                        }
                        else
                        {
                            _state = State::RGB;
                        }
                        break;

                    case State::RGB:
                    default:
                        _led_matrix.fillRgb(_red, _green, _blue);
                        break;
                    }

                    xSemaphoreGive(_mutex);
                }

                std::this_thread::sleep_for(sleep_time);
            }
        });

        _update_thread.detach();
    }

    std::thread _gif_frame_thread;
    std::atomic<bool> _gif_frame_thread_running{true};

    void provisionGifFrameThread()
    {
        ESP_LOGI(TAG, "Provisioning gif frame thread");
        auto cfg = esp_pthread_get_default_config();
        cfg.thread_name = "totem_gif_frame_thread";
        cfg.pin_to_core = 1;
        cfg.stack_size = 1024;
        cfg.prio = configMAX_PRIORITIES - 1;
        esp_pthread_set_cfg(&cfg);

        ESP_LOGI(TAG, "Starting gif frame thread");
        _gif_frame_thread = std::thread([this]
        {
            while (_gif_frame_thread_running)
            {
                if (_state == State::GIF)
                {
                    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
                    {
                        _gif_frame_idx = (_gif_frame_idx + 1) % _gif_total_frames;
                        xSemaphoreGive(_mutex);
                    }
                }

                const auto speed = std::max(_gif_speed.load(), MIN_GIF_SPEED);
                std::this_thread::sleep_for(std::chrono::milliseconds(speed));
            }
        });

        _gif_frame_thread.detach();
    }

    std::thread _brightness_thread;
    std::atomic<bool> _brightness_thread_running{true};

    void provisionBrightnessThread()
    {
        ESP_LOGI(TAG, "Provisioning brightness thread");
        auto cfg = esp_pthread_get_default_config();
        cfg.thread_name = "totem_brightness_thread";
        cfg.pin_to_core = 1;
        cfg.stack_size = 1024;
        cfg.prio = configMAX_PRIORITIES - 2;
        esp_pthread_set_cfg(&cfg);

        ESP_LOGI(TAG, "Starting brightness thread");
        const auto sleep_time = std::chrono::milliseconds(1000);
        _brightness_thread = std::thread([this, sleep_time]
        {
            while (_brightness_thread_running)
            {
                if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
                {
                    _led_matrix.setBrightness(_brightness);
                    xSemaphoreGive(_mutex);
                }

                std::this_thread::sleep_for(sleep_time);
            }
        });

        _brightness_thread.detach();
    }

public:
    explicit Totem(const LedMatrix& l) : _led_matrix(l)
    {
        if ((_mutex = xSemaphoreCreateMutex()) == nullptr)
        {
            ESP_LOGE(TAG, "Failed to create mutex for LED matrix");
            return;
        }

        provisionUpdateThread();
        provisionGifFrameThread();
        provisionBrightnessThread();
    }

    ~Totem()
    {
        ESP_LOGI(TAG, "Totem destructor called");

        _update_thread_running = false;
        if (_update_thread.joinable()) _update_thread.join();

        _gif_frame_thread_running = false;
        if (_gif_frame_thread.joinable()) _gif_frame_thread.join();

        _brightness_thread_running = false;
        if (_brightness_thread.joinable()) _brightness_thread.join();
    }

    void setRgbMode(const uint8_t r, const uint8_t g, const uint8_t b)
    {
        if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
        {
            _red = r;
            _green = g;
            _blue = b;
            _state = State::RGB;
            xSemaphoreGive(_mutex);
        }

        ESP_LOGI(TAG, "LED matrix set to solid color: R=%d, G=%d, B=%d", r, g, b);
    }

    void setGifMode(std::string buf, const size_t size)
    {
        const auto frames = size / LedMatrix::MATRIX_BUFFER_SIZE;

        if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
        {
            _gif_data = std::move(buf);
            _buffer = reinterpret_cast<uint8_t*>(&_gif_data[0]);
            _gif_total_frames = frames;
            _state = State::GIF;
            xSemaphoreGive(_mutex);
        }

        ESP_LOGI(TAG, "LED matrix set to GIF mode with frames=%d", frames);
    }

    void setSettings(const uint8_t brightness, const uint16_t speed)
    {
        _brightness = brightness;
        _gif_speed = speed;
        ESP_LOGI(TAG, "LED matrix brightness set to %d and speed set to %d", brightness, speed);
    }
};
