#pragma once

#include "matrix/MatrixGfx.hpp"

class TotemState
{
public:
    static constexpr TickType_t DEFAULT_RENDER_SPEED_MS = 16;
    static constexpr TickType_t DEFAULT_RENDER_TICK = pdMS_TO_TICKS(DEFAULT_RENDER_SPEED_MS);
    static constexpr TickType_t MIN_RENDER_TICK = 1;
    static constexpr uint8_t DEFAULT_BRIGHTNESS = 255;

private:
    std::atomic<size_t> render_speed_{DEFAULT_RENDER_TICK};
    std::atomic<uint8_t> brightness_{DEFAULT_BRIGHTNESS};

protected:
    TotemState()
    {
    }

public:
    virtual ~TotemState() = default;

    virtual void render(MatrixGfx& gfx) = 0;

    [[nodiscard]] TickType_t getRenderTick() const
    {
        return render_speed_.load();
    }

    void setRenderTick(const TickType_t tick)
    {
        const auto max = std::max(tick, MIN_RENDER_TICK);
        render_speed_.store(max);
    }

    [[nodiscard]] uint8_t getBrightness() const
    {
        return brightness_.load();
    }

    void setBrightness(const uint8_t brightness)
    {
        brightness_.store(brightness);
    }
};
