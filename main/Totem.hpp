#pragma once

#include <thread>
#include <cmath>

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

    State state_ = State::RGB;

    SemaphoreHandle_t _mutex = nullptr;

    static constexpr size_t MAP_GRANULARITY = ESP_HIMEM_BLKSZ;

    esp_himem_handle_t himem_ = nullptr;
    size_t himem_alloc_size_ = 0;
    size_t himem_gif_max_size_ = 0;
    size_t himem_gif_max_frames_ = 0;

    // Double buffering system - two sets of frame offsets
    size_t* gif_frame_offsets_active_;
    size_t* gif_frame_offsets_loading_;
    uint16_t gif_frame_idx_ = 0;
    uint16_t gif_total_frames_active_ = 0;
    uint16_t gif_total_frames_loading_ = 0;
    bool buffer_ready_to_swap_ = false;

    // Base offsets for the two buffer regions in HIMEM
    size_t buffer_a_base_offset_ = 0;
    size_t buffer_b_base_offset_ = 0;
    bool is_buffer_a_active_ = true;

    std::atomic<uint8_t> red_ = 0;
    std::atomic<uint8_t> green_ = 0;
    std::atomic<uint8_t> blue_ = 0;

    static constexpr uint16_t MIN_GIF_SPEED = 50;
    std::atomic<uint16_t> gif_speed_{MIN_GIF_SPEED};

    std::atomic<uint8_t> brightness_{255};

    std::thread update_thread_;
    std::atomic<bool> update_thread_running_{true};

    void initHimem()
    {
        const size_t mem_cnt = esp_himem_get_phys_size();
        const size_t mem_free = esp_himem_get_free_size();
        ESP_LOGI(TAG, "Himem has %d bytes of memory, %d bytes of which is free", mem_cnt, mem_free);

        himem_alloc_size_ = mem_free;

        // Split available memory into two equal parts for double buffering
        himem_gif_max_size_ = himem_alloc_size_ / 2;
        himem_gif_max_frames_ = himem_gif_max_size_ / LedMatrix::MATRIX_BUFFER_SIZE;

        // Define base offsets for each buffer
        buffer_a_base_offset_ = 0;
        buffer_b_base_offset_ = himem_gif_max_size_;

        if (const esp_err_t err = esp_himem_alloc(himem_alloc_size_, &himem_); err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to allocate %d bytes HIMEM: %s", himem_alloc_size_, esp_err_to_name(err));
            ESP_INFINITE_LOOP();
        }

        // Allocate arrays to store frame offsets for both buffers
        gif_frame_offsets_active_ = new size_t[himem_gif_max_frames_];
        gif_frame_offsets_loading_ = new size_t[himem_gif_max_frames_];

        ESP_LOGI(TAG, "HIMEM initialized for double buffering: %d bytes per buffer, max %d frames per buffer",
                 himem_gif_max_size_, himem_gif_max_frames_);
    }

    void provisionUpdateThread()
    {
        ESP_LOGI(TAG, "Provisioning update thread");

        esp_himem_rangehandle_t hr = nullptr;
        if (const esp_err_t err = esp_himem_alloc_map_range(MAP_GRANULARITY, &hr); err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to allocate %d bytes HIMEM: %s", MAP_GRANULARITY, esp_err_to_name(err));
            ESP_INFINITE_LOOP();
        }

        auto cfg = esp_pthread_get_default_config();
        cfg.thread_name = "totem_update_thread";
        cfg.pin_to_core = 1;
        cfg.stack_size = 4096;
        cfg.prio = configMAX_PRIORITIES;
        esp_pthread_set_cfg(&cfg);
        const auto sleep_time = std::chrono::milliseconds(MIN_GIF_SPEED);
        update_thread_ = std::thread([this, sleep_time, hr]
        {
            // Keep track of mapped blocks to ensure proper cleanup
            char* currently_mapped_block = nullptr;
            esp_err_t map_result = ESP_OK;

            while (update_thread_running_)
            {
                if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
                {
                    // Check if we need to swap buffers
                    if (buffer_ready_to_swap_)
                    {
                        // Make sure any currently mapped memory is unmapped before swapping
                        if (currently_mapped_block != nullptr)
                        {
                            esp_himem_unmap(hr, currently_mapped_block, MAP_GRANULARITY);
                            currently_mapped_block = nullptr;
                        }

                        // Swap active and loading buffers
                        std::swap(gif_frame_offsets_active_, gif_frame_offsets_loading_);
                        gif_total_frames_active_ = gif_total_frames_loading_;
                        gif_frame_idx_ = 0;
                        is_buffer_a_active_ = !is_buffer_a_active_;
                        buffer_ready_to_swap_ = false;

                        ESP_LOGI(TAG, "Swapped GIF buffers. Now using %s buffer with %d frames",
                                 is_buffer_a_active_ ? "A" : "B", gif_total_frames_active_);
                    }

                    switch (state_)
                    {
                    case State::GIF:
                        {
                            if (gif_total_frames_active_ > 0)
                            {
                                // Get current active buffer base
                                size_t active_buffer_base = is_buffer_a_active_
                                                                ? buffer_a_base_offset_
                                                                : buffer_b_base_offset_;

                                // Calculate which frame to display
                                size_t frame_offset = gif_frame_offsets_active_[gif_frame_idx_] + active_buffer_base;
                                // Calculate which block contains this frame
                                size_t block_offset = (frame_offset / MAP_GRANULARITY) * MAP_GRANULARITY;

                                // Ensure any previous mapping is unmapped first
                                if (currently_mapped_block != nullptr)
                                {
                                    esp_himem_unmap(hr, currently_mapped_block, MAP_GRANULARITY);
                                    currently_mapped_block = nullptr;
                                }

                                // Attempt to map the block
                                map_result = esp_himem_map(himem_,
                                                           hr,
                                                           block_offset,
                                                           0,
                                                           MAP_GRANULARITY,
                                                           0,
                                                           reinterpret_cast<void**>(&currently_mapped_block));

                                if (map_result != ESP_OK)
                                {
                                    ESP_LOGE(TAG, "Failed to map memory at offset %d: %s",
                                             block_offset, esp_err_to_name(map_result));
                                    // Skip this frame if mapping failed
                                    currently_mapped_block = nullptr;
                                }
                                else
                                {
                                    // Calculate offset within the mapped block
                                    size_t frame_offset_in_block = frame_offset % MAP_GRANULARITY;

                                    // Get pointer to the specific frame
                                    char* frame_ptr = currently_mapped_block + frame_offset_in_block;

                                    // Load the frame
                                    const auto frame_data = reinterpret_cast<volatile uint32_t*>(frame_ptr);
                                    LedMatrix::getInstance().loadFromBuffer(frame_data);

                                    // Keep block mapped until next iteration for efficiency
                                    // Will be unmapped at the start of the next iteration
                                }
                            }
                            else
                            {
                                state_ = State::RGB;
                            }
                        }
                        break;

                    case State::RGB:
                    default:
                        // Ensure any previous mapping is unmapped
                        if (currently_mapped_block != nullptr)
                        {
                            esp_himem_unmap(hr, currently_mapped_block, MAP_GRANULARITY);
                            currently_mapped_block = nullptr;
                        }

                        LedMatrix::getInstance().fillRgb(red_, green_, blue_);
                        break;
                    }

                    xSemaphoreGive(_mutex);
                }

                std::this_thread::sleep_for(sleep_time);
            }

            // Clean up
            if (currently_mapped_block != nullptr)
            {
                esp_himem_unmap(hr, currently_mapped_block, MAP_GRANULARITY);
            }

            esp_himem_free_map_range(hr);
        });
        update_thread_.detach();
    }

    std::thread gif_frame_thread_;
    std::atomic<bool> gif_frame_thread_running_{true};

    void provisionGifFrameThread()
    {
        ESP_LOGI(TAG, "Provisioning gif frame thread");
        auto cfg = esp_pthread_get_default_config();
        cfg.thread_name = "totem_gif_frame_thread";
        cfg.pin_to_core = 1;
        cfg.stack_size = 1024;
        cfg.prio = configMAX_PRIORITIES - 1;
        esp_pthread_set_cfg(&cfg);
        gif_frame_thread_ = std::thread([this]
        {
            while (gif_frame_thread_running_)
            {
                if (state_ == State::GIF)
                {
                    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
                    {
                        if (gif_total_frames_active_ > 0)
                        {
                            gif_frame_idx_ = (gif_frame_idx_ + 1) % gif_total_frames_active_;
                        }
                        xSemaphoreGive(_mutex);
                    }
                }

                const auto speed = std::max(gif_speed_.load(), MIN_GIF_SPEED);
                std::this_thread::sleep_for(std::chrono::milliseconds(speed));
            }
        });
        gif_frame_thread_.detach();
    }

    std::thread brightness_thread_;
    std::atomic<bool> brightness_thread_running_{true};

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
        brightness_thread_ = std::thread([this, sleep_time]
        {
            while (brightness_thread_running_)
            {
                if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
                {
                    LedMatrix::getInstance().setBrightness(brightness_);
                    xSemaphoreGive(_mutex);
                }

                std::this_thread::sleep_for(sleep_time);
            }
        });
        brightness_thread_.detach();
    }

public:
    Totem()
    {
        ESP_LOGI(TAG, "Initializing Totem...");

        if ((_mutex = xSemaphoreCreateMutex()) == nullptr)
        {
            ESP_LOGE(TAG, "Failed to create mutex for LED matrix");
            ESP_INFINITE_LOOP();
        }

        initHimem();

        provisionUpdateThread();
        provisionGifFrameThread();
        provisionBrightnessThread();
        ESP_LOGI(TAG, "Totem successfully initialized");
    }

    ~Totem()
    {
        ESP_LOGI(TAG, "Totem destructor called");

        update_thread_running_ = false;
        if (update_thread_.joinable()) update_thread_.join();

        gif_frame_thread_running_ = false;
        if (gif_frame_thread_.joinable()) gif_frame_thread_.join();

        brightness_thread_running_ = false;
        if (brightness_thread_.joinable()) brightness_thread_.join();

        if (himem_) esp_himem_free(himem_);
        if (gif_frame_offsets_active_) delete[] gif_frame_offsets_active_;
        if (gif_frame_offsets_loading_) delete[] gif_frame_offsets_loading_;
    }

    void setRgbMode(const uint8_t r, const uint8_t g, const uint8_t b)
    {
        if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
        {
            red_ = r;
            green_ = g;
            blue_ = b;
            state_ = State::RGB;
            xSemaphoreGive(_mutex);
        }

        ESP_LOGI(TAG, "LED matrix set to solid color: R=%d, G=%d, B=%d", r, g, b);
    }

    void setSettings(const uint8_t brightness, const uint16_t speed)
    {
        brightness_ = brightness;
        gif_speed_ = speed;
        ESP_LOGI(TAG, "LED matrix brightness set to %d and speed set to %d", brightness, speed);
    }

    esp_err_t handleGifRequest(httpd_req_t* req)
    {
        const size_t mem_cnt = esp_himem_get_phys_size();
        const size_t mem_free = esp_himem_get_free_size();
        ESP_LOGI(TAG, "Himem has %d bytes of memory, %d bytes of which is free", mem_cnt, mem_free);

        auto f = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        ESP_LOGI(TAG, "Free malloc: %d", f);

        char err_str[64];

        ESP_LOGI(TAG, "GIF request initiated with content length=%d", req->content_len);

        if (req->content_len == 0)
        {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content was empty");
            return ESP_FAIL;
        }

        if (req->content_len < LedMatrix::MATRIX_BUFFER_SIZE)
        {
            snprintf(err_str, sizeof(err_str), "GIF must be greater than %d", LedMatrix::MATRIX_BUFFER_SIZE);
            ESP_LOGE(TAG, "%s", err_str);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_str);
            return ESP_FAIL;
        }

        if (req->content_len % LedMatrix::MATRIX_BUFFER_SIZE != 0)
        {
            snprintf(err_str, sizeof(err_str), "GIF must be divisible by %d", LedMatrix::MATRIX_BUFFER_SIZE);
            ESP_LOGE(TAG, "%s", err_str);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_str);
            return ESP_FAIL;
        }

        const auto frames_count = req->content_len / LedMatrix::MATRIX_BUFFER_SIZE;
        if (frames_count > himem_gif_max_frames_)
        {
            snprintf(err_str, sizeof(err_str), "GIF must have less than %d frames", himem_gif_max_frames_);
            ESP_LOGE(TAG, "%s", err_str);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, err_str);
            return ESP_FAIL;
        }

        esp_himem_rangehandle_t hr = nullptr;
        if (const esp_err_t err = esp_himem_alloc_map_range(MAP_GRANULARITY, &hr); err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to allocate %d bytes HIMEM range: %s", MAP_GRANULARITY, esp_err_to_name(err));
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate HIMEM range");
            return ESP_FAIL;
        }

        int total_received = 0;
        char* current_block = nullptr;
        size_t current_block_offset = SIZE_MAX; // Initialize to invalid value

        // Determine which buffer to load into (the one that's not active)
        bool load_to_buffer_a = !is_buffer_a_active_;
        size_t loading_buffer_base = load_to_buffer_a ? buffer_a_base_offset_ : buffer_b_base_offset_;

        // Initialize frame offsets for the loading buffer (relative to buffer start)
        for (size_t i = 0; i < frames_count; i++)
        {
            gif_frame_offsets_loading_[i] = i * LedMatrix::MATRIX_BUFFER_SIZE;
        }

        ESP_LOGI(TAG, "Loading new GIF into %s buffer with %d frames",
                 load_to_buffer_a ? "A" : "B", frames_count);

        while (total_received < req->content_len)
        {
            // Calculate absolute offset in HIMEM for this chunk of data
            size_t absolute_offset = loading_buffer_base + total_received;

            // Calculate which block we need to map
            size_t block_to_map = (absolute_offset / MAP_GRANULARITY) * MAP_GRANULARITY;

            // If we need a new block, unmap the old one and map a new one
            if (current_block == nullptr || current_block_offset != block_to_map)
            {
                if (current_block != nullptr)
                {
                    esp_himem_unmap(hr, current_block, MAP_GRANULARITY);
                    current_block = nullptr;
                }

                // Map new block
                esp_err_t map_result = esp_himem_map(himem_,
                                                     hr,
                                                     block_to_map,
                                                     0,
                                                     MAP_GRANULARITY,
                                                     0,
                                                     reinterpret_cast<void**>(&current_block));

                if (map_result != ESP_OK)
                {
                    snprintf(err_str, sizeof(err_str), "Failed to map HIMEM at offset %zu: %s",
                             block_to_map, esp_err_to_name(map_result));
                    ESP_LOGE(TAG, "%s", err_str);
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_str);

                    if (current_block != nullptr)
                    {
                        esp_himem_unmap(hr, current_block, MAP_GRANULARITY);
                    }
                    esp_himem_free_map_range(hr);
                    return ESP_FAIL;
                }

                current_block_offset = block_to_map;
                ESP_LOGI(TAG, "Mapped new block at offset %zu", block_to_map);
            }

            // Calculate position within the block
            size_t position_in_block = absolute_offset % MAP_GRANULARITY;

            // Calculate remaining space in current block
            size_t space_in_block = MAP_GRANULARITY - position_in_block;

            // Calculate how much we need to receive
            size_t to_receive = std::min(req->content_len - total_received, space_in_block);

            // Receive data directly into the mapped memory
            const auto received = httpd_req_recv(req, current_block + position_in_block, to_receive);

            if (received == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue; // Retry if timeout occurred
            }

            if (received <= 0)
            {
                snprintf(err_str, sizeof(err_str), "Failed to receive content: %s", esp_err_to_name(received));
                ESP_LOGE(TAG, "%s", err_str);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, err_str);

                if (current_block != nullptr)
                {
                    esp_himem_unmap(hr, current_block, MAP_GRANULARITY);
                }

                esp_himem_free_map_range(hr);
                return ESP_FAIL;
            }

            total_received += received;
            ESP_LOGI(TAG, "Data received=%d, total=%d", received, total_received);
        }

        // Unmap the last block
        if (current_block != nullptr)
        {
            esp_himem_unmap(hr, current_block, MAP_GRANULARITY);
            current_block = nullptr;
        }

        esp_himem_free_map_range(hr);

        if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE)
        {
            // Store the frame count for the loading buffer
            gif_total_frames_loading_ = frames_count;

            // Signal that the buffer is ready to be swapped
            buffer_ready_to_swap_ = true;

            // If not in GIF mode already, switch to it immediately
            if (state_ != State::GIF)
            {
                state_ = State::GIF;
            }

            xSemaphoreGive(_mutex);
        }

        httpd_resp_sendstr(req, "LED Matrix GIF updated");
        ESP_LOGI(TAG, "New GIF loaded and ready to display on next frame");
        return ESP_OK;
    }
};
