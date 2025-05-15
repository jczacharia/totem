#pragma once

#include <atomic>

#include "MatrixGfx.hpp"

class PatternBase
{
public:
    static constexpr TickType_t DEFAULT_RENDER_SPEED_MS = 16;
    static constexpr TickType_t DEFAULT_RENDER_TICK = pdMS_TO_TICKS(DEFAULT_RENDER_SPEED_MS);
    static constexpr TickType_t MIN_RENDER_TICK = 1;

private:
    std::atomic<size_t> render_speed_{DEFAULT_RENDER_TICK};

protected:
    PatternBase()
    {
    }

public:
    virtual ~PatternBase() = default;

    virtual void render(MatrixGfx& gfx) = 0;

    [[nodiscard]] TickType_t getRenderTick() const
    {
        return render_speed_.load();
    }

    void setRenderTick(const TickType_t tick)
    {
        render_speed_.store(std::max(tick, MIN_RENDER_TICK));
    }
};
