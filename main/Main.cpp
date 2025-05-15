#include <chrono>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_psram.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "lwip/apps/netbiosns.h"

#include "state/LoadingState.hpp"
#include "state/Totem.hpp"
#include "state/GifState.hpp"
#include "state/WifiConnectingState.hpp"
#include "state/AudioSpectrumState.hpp"

#include "server/RestServer.hpp"

#define MDNS_HOST_NAME "esp-home"
#define MDNS_INSTANCE "totem server"

extern "C" void app_main(void)
{
    // Totem::setState<WifiConnectingState>();
    //
    // ESP_ERROR_CHECK(nvs_flash_init());
    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    //
    // mdns_init();
    // mdns_hostname_set(MDNS_HOST_NAME);
    // mdns_instance_name_set(MDNS_INSTANCE);
    //
    // mdns_txt_item_t serviceTxtData[] = {
    //     {"board", "esp32"},
    //     {"path", "/"}
    // };
    //
    // ESP_ERROR_CHECK(mdns_service_add(
    //     "ESP32-WebServer",
    //     "_http",
    //     "_tcp",
    //     80,
    //     serviceTxtData,
    //     std::size(serviceTxtData)));
    //
    // netbiosns_init();
    // netbiosns_set_name(MDNS_HOST_NAME);
    //
    // ESP_ERROR_CHECK(example_connect());
    // // ESP_ERROR_CHECK(init_fs());
    //
    // const RestServer& server = RestServer::getInstance();
    //
    // server.registerEndpoint("/api/matrix/state/gif", HTTP_POST, GifState::endpoint);
    // server.registerEndpoint("/api/matrix/state/loading", HTTP_POST, LoadingState::endpoint);
    // server.registerEndpoint("/api/matrix/state/audio-spectrum", HTTP_POST, AudioSpectrumState::endpoint);

    Totem::setState<AudioSpectrumState>();
    vTaskDelete(nullptr);
}
