#pragma once

#include <thread>

#include "esp_pthread.h"
#include "esp32/himem.h"

#include "LedMatrix.hpp"
#include "Microphone.hpp"

class Totem
{
    static constexpr auto TAG = "Totem";

    enum class State
    {
        RGB,
        GIF,
    };

    State _state = State::RGB;

    SemaphoreHandle_t _mutex = nullptr;

    static constexpr uint8_t MAX_GIF_FRAMES = 200;
    char* _gif_frames[MAX_GIF_FRAMES];
    uint16_t _gif_frame_idx = 0;
    uint16_t _gif_total_frames = 0;

    std::atomic<uint8_t> _red = 0;
    std::atomic<uint8_t> _green = 0;
    std::atomic<uint8_t> _blue = 0;

    static constexpr uint16_t MIN_GIF_SPEED = 50;
    std::atomic<uint16_t> _gif_speed{MIN_GIF_SPEED};

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
                        if (const auto frame = _gif_frames[_gif_frame_idx]; frame != nullptr)
                        {
                            const auto frame_as_buf = reinterpret_cast<uint32_t*>(frame);
                            LedMatrix::getInstance().loadFromBuffer(frame_as_buf);
                        }
                        else
                        {
                            _state = State::RGB;
                        }
                        break;

                    case State::RGB:
                    default:
                        LedMatrix::getInstance().fillRgb(_red, _green, _blue);
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
        const auto sleep_time = std::chrono::milliseconds(1000);
        _brightness_thread = std::thread([this, sleep_time]
        {
            while (_brightness_thread_running)
            {
                if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
                {
                    LedMatrix::getInstance().setBrightness(_brightness);
                    xSemaphoreGive(_mutex);
                }

                std::this_thread::sleep_for(sleep_time);
            }
        });
        _brightness_thread.detach();
    }

public:
    // explicit Totem(const LedMatrix& l, const Microphone& m) : _led_matrix(l), _microphone(m)
    Totem()
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

    void setSettings(const uint8_t brightness, const uint16_t speed)
    {
        _brightness = brightness;
        _gif_speed = speed;
        ESP_LOGI(TAG, "LED matrix brightness set to %d and speed set to %d", brightness, speed);
    }

    // TODO GIFDEC.H
    esp_err_t handleGifRequest(httpd_req_t* req)
    {
        if (req->content_len == 0)
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content was empty");
            return ESP_FAIL;
        }

        if (req->content_len < LedMatrix::MATRIX_BUFFER_SIZE)
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "GIF buffer must be greater than 16384");
            return ESP_FAIL;
        }

        if (req->content_len % LedMatrix::MATRIX_BUFFER_SIZE != 0)
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "GIF buffer must be divisible by 16384");
            return ESP_FAIL;
        }

        if (const auto frames = req->content_len / LedMatrix::MATRIX_BUFFER_SIZE; frames > MAX_GIF_FRAMES)
        {
            httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Too many GIF frames, max is 200");
            return ESP_FAIL;
        }

        const size_t mem_cnt = esp_himem_get_phys_size();
        const size_t mem_free = esp_himem_get_free_size();
        ESP_LOGI(
            TAG,
            "GIF request initiated: Received content with length=%d, Himem has %dKiB of memory, %dKiB of which is free",
            req->content_len,
            static_cast<int>(mem_cnt) / 1024, static_cast<int>(mem_free) / 1024);

        esp_himem_handle_t mh = nullptr;
        esp_himem_rangehandle_t rh = nullptr;
        esp_err_t err = ESP_OK;
        char err_buf[128];

        constexpr size_t map_granularity = ESP_HIMEM_BLKSZ;
        const size_t min_alloc = std::min(req->content_len, static_cast<size_t>(ESP_HIMEM_BLKSZ));

        // 1. Allocate physical Himem for the entire content
        err = esp_himem_alloc(min_alloc, &mh);
        if (err != ESP_OK)
        {
            snprintf(err_buf, sizeof(err_buf), "Failed to allocate %zu bytes in Himem: %s", min_alloc,
                     esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_buf);
            return ESP_FAIL;
        }


        // 2. Allocate a block of address range to map Himem into ESP_HIMEM_BLKSZ
        err = esp_himem_alloc_map_range(map_granularity, &rh);
        if (err != ESP_OK)
        {
            esp_himem_free(mh);
            snprintf(err_buf, sizeof(err_buf), "Failed to allocate Himem map range: %s", esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_buf);
            return ESP_FAIL;
        }

        char* frames_buf[MAX_GIF_FRAMES];
        int total_received = 0;
        uint frame_count = 0;

        // 3. Map himen memory to gif frames
        while (total_received < min_alloc)
        {
            const auto chunk_size = std::min(map_granularity, min_alloc - total_received);
            char* frame = nullptr;
            err = esp_himem_map(mh, rh,
                                total_received - (total_received % map_granularity),
                                0,
                                map_granularity,
                                0,
                                reinterpret_cast<void**>(&frame));
            if (err != ESP_OK)
            {
                esp_himem_free(mh);
                esp_himem_free_map_range(rh);
                snprintf(err_buf, sizeof(err_buf), "Failed to map himem at offset %d: %s",
                         total_received,
                         esp_err_to_name(err));
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_buf);
                return ESP_FAIL;
            }

            const int offset_in_block = total_received % map_granularity;
            const int received = httpd_req_recv(req, frame + offset_in_block, chunk_size);
            if (received <= 0)
            {
                esp_himem_free(mh);
                esp_himem_free_map_range(rh);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data from HTTP request");
                return ESP_FAIL;
            }

            frames_buf[frame_count++] = frame;
            esp_himem_unmap(rh, frame, map_granularity);
            total_received += received;
        }

        esp_himem_free_map_range(rh);
        esp_himem_free(mh);

        if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
        {
            for (size_t i = 0; i < frame_count; i++)
            {
                _gif_frames[i] = frames_buf[i];
            }

            _gif_total_frames = frame_count;
            _state = State::GIF;

            xSemaphoreGive(_mutex);
        }

        httpd_resp_sendstr(req, "LED Matrix GIF updated");
        ESP_LOGI(TAG, "LED matrix set to GIF mode with frames=%d", frame_count);
        return ESP_OK;
    }
};
