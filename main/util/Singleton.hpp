#pragma once

#include <mutex>
#include <memory>
#include <atomic>

namespace util
{
    template <typename T>
    class Singleton
    {
        inline static std::atomic<T*> _instance_ptr{nullptr};
        inline static std::mutex _mutex;

    protected:
        Singleton() = default;
        virtual ~Singleton() = default;

    public:
        Singleton(const Singleton&) = delete;
        Singleton& operator =(const Singleton&) = delete;
        Singleton(Singleton&&) = delete;
        Singleton& operator =(Singleton&&) = delete;

        static T& getInstance()
        {
            auto instance = _instance_ptr.load(std::memory_order_acquire);
            if (!instance)
            {
                std::lock_guard lock(_mutex);
                instance = _instance_ptr.load(std::memory_order_relaxed);
                if (!instance)
                {
                    instance = new T();
                    _instance_ptr.store(instance, std::memory_order_release);
                }
            }
            return *instance;
        }

        static void destroy()
        {
            std::lock_guard lock(_mutex);
            if (auto instance = _instance_ptr.load(std::memory_order_relaxed))
            {
                delete instance;
                _instance_ptr.store(nullptr, std::memory_order_relaxed);
            }
        }
    };
}
