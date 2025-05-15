#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <string>

#include "util/ThreadManager.hpp"
#include "util/Singleton.hpp"

#include "matrix/MatrixGfx.hpp"

#include "TotemState.hpp"

// A layer represents a collection of states that are rendered in sequence
class StateLayer
{
    std::string name_;
    float opacity_ = 1.0f;
    std::shared_ptr<TotemState> current_state_;

public:
    explicit StateLayer(const std::string& name, const float opacity = 1.0f)
        : name_(name), opacity_(opacity)
    {
    }

    void setOpacity(const float opacity)
    {
        opacity_ = std::clamp(opacity, 0.0f, 1.0f);
    }

    float getOpacity() const { return opacity_; }

    const std::string& getName() const { return name_; }

    std::shared_ptr<TotemState> getCurrentState() const
    {
        return current_state_;
    }

    void setState(const std::shared_ptr<TotemState>& state)
    {
        current_state_ = state;
    }

    template <typename TState, typename... TArgs>
    void setState(TArgs&&... args)
    {
        current_state_ = std::make_shared<TState>(std::forward<TArgs>(args)...);
    }
};
