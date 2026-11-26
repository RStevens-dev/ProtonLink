#include "telemetry.h"
#include <string.h>
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define MAX_STATUS 4096
static SemaphoreHandle_t mutex;
static protonlink_status_t latest;
static int64_t received_us;
static const char *TAG = "protonlink";
static volatile bool connected;
static char response[MAX_STATUS + 1];

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) connected = false;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) connected = true;
}
static bool fetch_status(protonlink_status_t *out)
{
    esp_http_client_config_t cfg = {
        .url = CONFIG_PROTONLINK_STATUS_URL,
        .timeout_ms = 2500,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;
    bool ok = false;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        int64_t length = esp_http_client_fetch_headers(client);
        if (length >= 0 && length <= MAX_STATUS && esp_http_client_get_status_code(client) == 200) {
            int used = 0;
            while (used < MAX_STATUS + 1) {
                int n = esp_http_client_read(client, response + used, MAX_STATUS + 1 - used);
                if (n <= 0) break;
                used += n;
            }
            if (used <= MAX_STATUS && esp_http_client_is_complete_data_received(client)) {
                response[used] = 0;
                ok = protonlink_decode(response, out);
            }
        }
        esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);
    return ok;
}
static void poll_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (!connected) {
            esp_wifi_connect();
        } else {
            protonlink_status_t sample;
            if (fetch_status(&sample)) {
                xSemaphoreTake(mutex, portMAX_DELAY);
                latest = sample;
                received_us = esp_timer_get_time();
                xSemaphoreGive(mutex);
            } else {
                xSemaphoreTake(mutex, portMAX_DELAY);
                received_us = 0;
                xSemaphoreGive(mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
bool protonlink_snapshot(protonlink_status_t *out)
{
    if (!mutex || !out || !connected) return false;
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool fresh = received_us > 0 && esp_timer_get_time() - received_us < 6000000;
    if (fresh) *out = latest;
    xSemaphoreGive(mutex);
    return fresh;
}
void protonlink_network_start(void)
{
    if (!strlen(CONFIG_PROTONLINK_WIFI_SSID)) {
        ESP_LOGI(TAG, "Wi-Fi not configured; staying offline");
        return;
    }
    mutex = xSemaphoreCreateMutex();
    if (!mutex) return;
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL));
    wifi_config_t wifi = {0};
    if (strlen(CONFIG_PROTONLINK_WIFI_SSID) >= sizeof(wifi.sta.ssid) ||
        strlen(CONFIG_PROTONLINK_WIFI_PASSWORD) >= sizeof(wifi.sta.password)) {
        ESP_LOGE(TAG, "Wi-Fi configuration too long");
        return;
    }
    strcpy((char *)wifi.sta.ssid, CONFIG_PROTONLINK_WIFI_SSID);
    strcpy((char *)wifi.sta.password, CONFIG_PROTONLINK_WIFI_PASSWORD);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
    ESP_ERROR_CHECK(esp_wifi_start());
    if (xTaskCreate(poll_task, "gpstar_poll", 6144, NULL, 4, NULL) != pdPASS)
        ESP_LOGE(TAG, "Unable to start telemetry task");
}
