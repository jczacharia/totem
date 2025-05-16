#include <chrono>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_psram.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "lwip/apps/netbiosns.h"

#include "Totem.hpp"
#include "RestServer.hpp"

#include "patterns/AudioSpectrumPattern.hpp"
#include "playlists/DefaultPlaylist.hpp"
#include "patterns/WifiConnectingPattern.hpp"

#define MDNS_HOST_NAME "esp-home"
#define MDNS_INSTANCE "totem server"


extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(MatrixDriver::start());
    ESP_ERROR_CHECK(Microphone::start());
    ESP_ERROR_CHECK(Totem::start());

    // Register only essential patterns for initial testing
    PatternRegistry::add_pattern<AudioSpectrumPattern>();
    PatternRegistry::add_pattern<FirePattern>();
    PatternRegistry::add_pattern<WifiConnectingPattern>();
    PatternRegistry::add_pattern<DefaultPlaylist>();

    // PatternRegistry::add_pattern<Playlist>();

    // Set DefaultPlaylist as the startup pattern
    Totem::set_pattern<DefaultPlaylist>();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // In app_main function or early initialization code

    // You can uncomment these once DefaultPlaylist is working properly
    // PatternRegistry::add_pattern<FirePattern>();
    // PatternRegistry::add_pattern<LoadingPattern>();
    // PatternRegistry::add_pattern<SolidColorPattern>();
    // PatternRegistry::add_pattern<SinglePixelPattern>();

    // ESP_ERROR_CHECK(mdns_init());
    // ESP_ERROR_CHECK(mdns_hostname_set(MDNS_HOST_NAME));
    // ESP_ERROR_CHECK(mdns_instance_name_set(MDNS_INSTANCE));
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
    // ESP_ERROR_CHECK(server.start());
    // vTaskDelete(nullptr);
}
