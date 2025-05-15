#pragma once

#include <complex>
#include <expected>

#include "esp_attr.h"
#include "esp_http_server.h"
#include "esp_timer.h"

namespace util::time
{
    inline unsigned long IRAM_ATTR millis()
    {
        return static_cast<unsigned long>(esp_timer_get_time() / 1000ULL);
    }
}
