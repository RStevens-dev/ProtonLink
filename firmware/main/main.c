#include "esp_err.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "telemetry.h"

static lv_obj_t *status_label;
static lv_obj_t *telemetry_label;
static void refresh(lv_timer_t *timer)
{
    (void)timer;
    protonlink_status_t s;
    if (!protonlink_snapshot(&s)) {
        lv_label_set_text(telemetry_label, "GPStar offline / stale");
        return;
    }
    lv_label_set_text_fmt(telemetry_label, "Pack: %s | %s\nMaster: %d%%  FX: %d%%  Music: %d%%\n%s / %s", s.pack_on ? "ON" : "OFF", s.firing ? "FIRING" : "IDLE", s.master, s.effects, s.music, s.temperature, s.cyclotron);
}
static void touch_test(lv_event_t *event)
{
    (void)event;
    lv_label_set_text(status_label, "Touch OK | FIELD LOCK");
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    if (!bsp_display_start()) return;
    bsp_display_lock(0);
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101418), 0);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "PROTONLINK");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xE5E7EB), 0);
    status_label = lv_label_create(screen);
    lv_label_set_text(status_label, "OFFLINE | FIELD LOCK");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0xF0B44C), 0);
    lv_obj_t *button = lv_button_create(screen);
    lv_obj_set_size(button, 260, 70);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, "TEST TOUCH");
    lv_obj_center(label);
    lv_obj_add_event_cb(button, touch_test, LV_EVENT_CLICKED, NULL);
    telemetry_label = lv_label_create(screen);
    lv_obj_set_style_text_color(telemetry_label, lv_color_hex(0xE5E7EB), 0);
    lv_obj_set_style_text_align(telemetry_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(telemetry_label, "GPStar offline / stale");
    lv_timer_create(refresh, 500, NULL);
    bsp_display_unlock();
    protonlink_network_start();
}
