#include "esp_err.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "telemetry.h"
#include "ui.h"
void app_main(void)
{
    esp_err_t err=nvs_flash_init();
    if (err==ESP_ERR_NVS_NO_FREE_PAGES || err==ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err=nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    if (!bsp_display_start()) return;
    bsp_display_lock(0);
    protonlink_ui_create();
    bsp_display_unlock();
    protonlink_network_start();
}
