#include "telemetry.h"
#include "control.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdlib.h>
#include "freertos/task.h"
#include "freertos/semphr.h"

#define MAX_STATUS 4096
static SemaphoreHandle_t mutex;
static protonlink_status_t latest;
static int64_t received_us;
static const char *TAG = "protonlink";
static atomic_bool connected;
static atomic_bool unlocked;
static QueueHandle_t requests;
static atomic_uint lock_epoch;
static atomic_bool busy;
typedef struct { pl_action_t action; int channel; int target; bool volume; unsigned epoch; protonlink_status_t before; } command_request_t;
static char response[MAX_STATUS + 1];

static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) { connected = false; pl_set_unlocked(false); }
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
bool pl_is_unlocked(void) { return atomic_load(&unlocked); }
void pl_set_unlocked(bool value)
{
#ifdef CONFIG_PROTONLINK_ENABLE_CONTROL
    if (value && connected) { atomic_store(&unlocked, true); return; }
#else
    (void)value;
#endif
    atomic_store(&unlocked, false);
    atomic_fetch_add(&lock_epoch, 1);
}
bool pl_busy(void) { return atomic_load(&busy); }
static bool enqueue(command_request_t *request)
{
    if (atomic_exchange(&busy,true)) return false;
    if (xQueueSend(requests,request,0)==pdTRUE) return true;
    atomic_store(&busy,false);
    return false;
}
bool pl_request(pl_action_t action)
{
    command_request_t request = {.action=action,.epoch=atomic_load(&lock_epoch)};
    if (!requests || !pl_is_unlocked() || !protonlink_snapshot(&request.before) ||
        !pl_command_path(action, &request.before)) return false;
    return enqueue(&request);
}
bool pl_request_volume(int channel, int target)
{
    command_request_t req={.volume=true,.channel=channel,.target=target,.epoch=atomic_load(&lock_epoch)};
    if (!requests || !pl_is_unlocked() || !protonlink_snapshot(&req.before) || !pl_volume_step(&req.before,channel,target)) return false;
    return enqueue(&req);
}
static bool send_path(command_request_t *req,const char *path)
{
    if (!path || !pl_is_unlocked() || req->epoch != atomic_load(&lock_epoch) || !connected) return false;
    const char *base = CONFIG_PROTONLINK_STATUS_URL;
    size_t n = strlen(base);
    if (n < 7 || strcmp(base+n-7, "/status") || strncmp(base, "http://", 7)) return false;
    char url[256];
    if (n-7+strlen(path) >= sizeof(url)) return false;
    snprintf(url,sizeof(url),"%.*s%s",(int)(n-7),base,path);
    esp_http_client_config_t cfg = {.url=url,.timeout_ms=2500,.disable_auto_redirect=true,.method=HTTP_METHOD_PUT};
    esp_http_client_handle_t client=esp_http_client_init(&cfg);
    if (!client) return false;
    bool ok=false;
    if (pl_is_unlocked() && connected && req->epoch == atomic_load(&lock_epoch))
        ok=esp_http_client_perform(client)==ESP_OK && esp_http_client_get_status_code(client)==200;
    esp_http_client_cleanup(client);
    return ok;
}
static void publish(const protonlink_status_t *sample)
{
    xSemaphoreTake(mutex,portMAX_DELAY);
    latest=*sample;
    received_us=esp_timer_get_time();
    xSemaphoreGive(mutex);
}
static void execute_request(command_request_t *req)
{
    protonlink_status_t fresh;
    if (!pl_is_unlocked() || req->epoch!=atomic_load(&lock_epoch) || !fetch_status(&fresh)) return;
    publish(&fresh);
    if (!req->volume) {
        const char *path=pl_command_path(req->action,&fresh);
        if ((req->action==PL_PLAY_STOP || req->action==PL_PAUSE_RESUME) &&
            (fresh.playing!=req->before.playing || fresh.paused!=req->before.paused)) return;
        send_path(req,path);
        return;
    }
    for (int step=0;step<20;step++) {
        int prior=pl_volume_value(&fresh,req->channel);
        const char *path=pl_volume_step(&fresh,req->channel,req->target);
        if (!path || !send_path(req,path)) break;
        bool advanced=false;
        for (int attempt=0;attempt<4;attempt++) {
            vTaskDelay(pdMS_TO_TICKS(200));
            if (!pl_is_unlocked() || req->epoch!=atomic_load(&lock_epoch) || !connected) return;
            if (!fetch_status(&fresh)) { pl_set_unlocked(false); return; }
            publish(&fresh);
            int current=pl_volume_value(&fresh,req->channel);
            if (current!=prior) {
                advanced=abs(current-req->target)<abs(prior-req->target) &&
                    ((prior<req->target && current<=req->target) || (prior>req->target && current>=req->target));
                break;
            }
        }
        if (!advanced) break;
    }
}
static void poll_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (!connected) {
            command_request_t abandoned;
            if (requests && xQueueReceive(requests,&abandoned,0)==pdTRUE) atomic_store(&busy,false);
            esp_wifi_connect();
        } else {
            command_request_t request;
            if (requests && xQueueReceive(requests,&request,0)==pdTRUE) { execute_request(&request); atomic_store(&busy,false); }
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
                pl_set_unlocked(false);
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
    requests = xQueueCreate(1,sizeof(command_request_t));
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
