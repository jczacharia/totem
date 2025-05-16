#pragma once

#include <unordered_map>
#include <string>
#include <functional>
#include <memory>

#include "PatternBase.hpp"
#include "esp_log.h"

class PatternRegistry final
{
    static constexpr auto TAG = "PatternRegistry";

    static std::unordered_map<std::string, std::function<std::shared_ptr<PatternBase>()>> pattern_factories_;

public:
    PatternRegistry() = delete;

    template <typename TPattern>
    static void add_pattern()
    {
        auto temp_instance = std::make_shared<TPattern>();
        const std::string name = temp_instance->get_name();
        pattern_factories_[name] = [] { return std::make_shared<TPattern>(); };
        ESP_LOGI(TAG, "Pattern registered: %s", name.c_str());
    }

    static std::shared_ptr<PatternBase> create_pattern(const std::string& name)
    {
        if (const auto it = pattern_factories_.find(name); it != pattern_factories_.end())
        {
            return it->second();
        }

        ESP_LOGW(TAG, "Pattern '%s' not found", name.c_str());
        return nullptr;
    }

    static std::vector<std::string> get_pattern_names()
    {
        std::vector<std::string> names;
        for (const auto& key : pattern_factories_ | std::views::keys)
        {
            names.push_back(key);
        }
        return names;
    }

    static bool is_pattern_registered(const std::string& name)
    {
        return pattern_factories_.contains(name);
    }
};

std::unordered_map<std::string, std::function<std::shared_ptr<PatternBase>()>> PatternRegistry::pattern_factories_;
