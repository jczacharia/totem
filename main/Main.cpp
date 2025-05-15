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

#include "patterns/audio/AudioSpectrumPattern.hpp"

#include "patterns/static/GifPattern.hpp"
#include "patterns/static/LoadingPattern.hpp"
#include "patterns/static/SinglePixelPattern.hpp"
#include "patterns/static/SolidColorPattern.hpp"
#include "patterns/static/WifiConnectingPattern.hpp"

#define MDNS_HOST_NAME "esp-home"
#define MDNS_INSTANCE "totem server"
static constexpr auto TAG = "Main";

LedMatrix matrix;
Microphone microphone;
MatrixGfx gfx(matrix, microphone);
Totem totem(gfx);
RestServer server;

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(matrix.start());
    ESP_ERROR_CHECK(microphone.start());
    ESP_ERROR_CHECK(totem.start());
    totem.set_state<LoadingPattern>();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(mdns_init());
    ESP_ERROR_CHECK(mdns_hostname_set(MDNS_HOST_NAME));
    ESP_ERROR_CHECK(mdns_instance_name_set(MDNS_INSTANCE));

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

    ESP_ERROR_CHECK(server.start());

    // Totem settings

    ESP_ERROR_CHECK(server.register_endpoint(
        "/api/brightness",
        HTTP_POST,
        Totem::brightness_endpoint ,
        &totem));

    // Audio Patterns

    ESP_ERROR_CHECK(server.register_endpoint(
        "/api/pattern/audio-spectrum",
        HTTP_POST,
        AudioSpectrumPattern::endpoint,
        &totem));

    // Static Patterns

    ESP_ERROR_CHECK(server.register_endpoint(
        "/api/pattern/gif",
        HTTP_POST,
        GifPattern::endpoint,
        &totem));

    ESP_ERROR_CHECK(server.register_endpoint(
        "/api/pattern/loading",
        HTTP_POST,
        LoadingPattern::endpoint,
        &totem));

    ESP_ERROR_CHECK(server.register_endpoint(
        "/api/pattern/single-pixel",
        HTTP_POST,
        SinglePixelPattern::endpoint,
        &totem));

    ESP_ERROR_CHECK(server.register_endpoint(
        "/api/pattern/solid-color",
        HTTP_POST,
        SolidColorPattern::endpoint,
        &totem));

    ESP_ERROR_CHECK(server.register_endpoint(
        "/api/pattern/wifi-connecting",
        HTTP_POST,
        WifiConnectingPattern::endpoint,
        &totem));


    totem.set_state<AudioSpectrumPattern>();
    ESP_LOGI(TAG, "Totem is running!");
    vTaskDelete(nullptr);
}
