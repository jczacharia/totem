#include <chrono>
#include <thread>
#include "esp_event.h"
#include "esp_psram.h"
#include "esp_netif.h"
#include "esp_vfs_fat.h"
#include "LedMatrix.hpp"
#include "mdns.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "RestServer.hpp"
#include "Totem.hpp"
#include "Microphone.hpp"

#include "lwip/apps/netbiosns.h"

#define MDNS_HOST_NAME "esp-home"
#define MDNS_INSTANCE "totem server"

static constexpr auto TAG = "Main";

extern "C" void app_main(void)
{
    const size_t mem_cnt = esp_himem_get_phys_size();
    const size_t mem_free = esp_himem_get_free_size();
    ESP_LOGI(TAG, "Himem has %dKiB of memory, %dKiB of which is free",
             static_cast<int>(mem_cnt) / 1024, static_cast<int>(mem_free) / 1024);

    LedMatrix::getInstance();
    Microphone::getInstance();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    mdns_init();
    mdns_hostname_set(MDNS_HOST_NAME);
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add(
        "ESP32-WebServer",
        "_http",
        "_tcp",
        80,
        serviceTxtData,
        std::size(serviceTxtData)));

    netbiosns_init();
    netbiosns_set_name(MDNS_HOST_NAME);

    ESP_ERROR_CHECK(example_connect());
    // ESP_ERROR_CHECK(init_fs());

    Totem totem;
    RestServer::Start(totem);
    std::this_thread::sleep_for(std::chrono::years::max());
}
