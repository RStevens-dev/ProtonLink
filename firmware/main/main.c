#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "telemetry.h"
#include "ui.h"
static void report_memory(const char *stage)
{
    const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const uint32_t external = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    ESP_LOGI("ProtonLink", "%s: internal free=%zu largest=%zu minimum=%zu; PSRAM free=%zu largest=%zu minimum=%zu",
             stage, heap_caps_get_free_size(internal), heap_caps_get_largest_free_block(internal),
             heap_caps_get_minimum_free_size(internal), heap_caps_get_free_size(external),
             heap_caps_get_largest_free_block(external), heap_caps_get_minimum_free_size(external));
}
void app_main(void)
{
    esp_err_t err=nvs_flash_init();
    if (err==ESP_ERR_NVS_NO_FREE_PAGES || err==ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err=nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    report_memory("Before display");
    if (!bsp_display_start()) return;
    bsp_display_lock(0);
    protonlink_ui_create();
    bsp_display_unlock();
    report_memory("After UI creation");
    protonlink_network_start();
    report_memory("After network start");
}
