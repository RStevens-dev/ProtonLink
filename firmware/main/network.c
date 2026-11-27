#include "telemetry.h"
#include "control.h"
#include "catalog.h"
#include "options.h"
#include "cJSON.h"
#include "json_input.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_attr.h"
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
typedef struct { pl_action_t action; int channel; int target; bool volume; bool select_track; bool option; pl_option_t option_id; unsigned epoch; protonlink_status_t before; } command_request_t;
static char response[MAX_STATUS + 1];
static EXT_RAM_BSS_ATTR char song_response[PL_SONG_LIST_SIZE+2048], songs[PL_SONG_LIST_SIZE];
static pl_sync_state_t song_state;
static int song_first, song_last;
static atomic_uint connection_generation;
static unsigned capabilities_generation;
static uint32_t advertised_options;
static bool modern_pack;
static enum { PROBE_ROUTES, PROBE_REQUEST_PACK, PROBE_READ_PACK, PROBE_COMPLETE } probe_phase;
static unsigned probe_attempts;
static unsigned received_generation;

static bool endpoint_url(const char *path, char url[256])
{
    const char *base=CONFIG_PROTONLINK_STATUS_URL;
    size_t length=strlen(base);
    if(length<7 || strcmp(base+length-7,"/status") || strncmp(base,"http://",7) ||
       length-7+strlen(path)>=256) return false;
    snprintf(url,256,"%.*s%s",(int)(length-7),base,path);
    return true;
}

static bool fetch_json(const char *url, char *buffer, size_t capacity)
{
    esp_http_client_config_t cfg={.url=url,.timeout_ms=2500,.disable_auto_redirect=true};
    esp_http_client_handle_t client=esp_http_client_init(&cfg);
    if(!client) return false;
    bool ok=false;
    if(esp_http_client_open(client,0)==ESP_OK) {
        int64_t length=esp_http_client_fetch_headers(client);
        if(length>=0 && (uint64_t)length<capacity && esp_http_client_get_status_code(client)==200) {
            size_t used=0;
            while(used<capacity) {
                int n=esp_http_client_read(client,buffer+used,capacity-used);
                if(n<=0) break;
                used+=(size_t)n;
            }
            if(used<capacity && esp_http_client_is_complete_data_received(client)) {
                buffer[used]=0;
                ok=true;
            }
        }
        esp_http_client_close(client);
    }
    esp_http_client_cleanup(client);
    return ok;
}

bool protonlink_request_song_list(void)
{
    protonlink_status_t s;
    if(!protonlink_snapshot(&s) || !pl_catalog_range(s.track_first,s.track_last)) return false;
    xSemaphoreTake(mutex,portMAX_DELAY);
    song_state=PL_SYNC_PENDING;
    xSemaphoreGive(mutex);
    return true;
}

pl_sync_state_t protonlink_song_list(char *out, size_t capacity, int *first, int *last)
{
    if(!mutex) return PL_SYNC_UNAVAILABLE;
    xSemaphoreTake(mutex,portMAX_DELAY);
    pl_sync_state_t state=song_state;
    if(state==PL_SYNC_READY && out) {
        size_t length=strlen(songs);
        if(length>=capacity) state=PL_SYNC_UNAVAILABLE;
        else {
            memcpy(out,songs,length+1);
            *first=song_first;
            *last=song_last;
        }
        song_state=PL_SYNC_IDLE;
    }
    xSemaphoreGive(mutex);
    return state;
}

static void sync_songs(const protonlink_status_t *sample)
{
    char url[256];
    unsigned generation=atomic_load(&connection_generation);
    cJSON *root=NULL;
    if(pl_catalog_range(sample->track_first,sample->track_last) &&
       endpoint_url("/config/device",url) && fetch_json(url,song_response,sizeof(song_response)))
        root=pl_json_depth_ok(song_response)?cJSON_ParseWithOpts(song_response,NULL,true):NULL;
    const cJSON *list=cJSON_GetObjectItemCaseSensitive(root,"songList");
    xSemaphoreTake(mutex,portMAX_DELAY);
    size_t length=cJSON_IsString(list)?strlen(list->valuestring):0;
    song_state=PL_SYNC_UNAVAILABLE;
    if(connected && generation==atomic_load(&connection_generation) && length && length<sizeof(songs)) {
        memcpy(songs,list->valuestring,length+1);
        song_first=sample->track_first;
        song_last=sample->track_last;
        song_state=PL_SYNC_READY;
    }
    xSemaphoreGive(mutex);
    cJSON_Delete(root);
}

static bool has_put(const cJSON *paths,const char *path)
{
    const cJSON *route=cJSON_GetObjectItemCaseSensitive(paths,path);
    return cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(route,"put"));
}

static void probe_capabilities(void)
{
    unsigned generation=atomic_load(&connection_generation);
    if(capabilities_generation!=generation) {
        capabilities_generation=generation;
        advertised_options=0;
        modern_pack=false;
        probe_phase=PROBE_ROUTES;
    }
    char url[256];
    switch(probe_phase) {
    case PROBE_ROUTES:
        if(endpoint_url("/openapi.json",url) && fetch_json(url,song_response,sizeof(song_response))) {
            cJSON *root=pl_json_depth_ok(song_response)?cJSON_ParseWithOpts(song_response,NULL,true):NULL;
            const cJSON *paths=cJSON_GetObjectItemCaseSensitive(root,"paths");
            for(int i=0;i<PL_OPT_COUNT;i++) {
                const char *first=pl_option_candidate(i,false), *second=pl_option_candidate(i,true);
                if(has_put(paths,first) && (!second || has_put(paths,second))) advertised_options|=1u<<i;
            }
            cJSON_Delete(root);
        }
        probe_phase=advertised_options?PROBE_REQUEST_PACK:PROBE_COMPLETE;
        break;
    case PROBE_REQUEST_PACK:
        if(endpoint_url("/settings/pack",url)) fetch_json(url,song_response,sizeof(song_response));
        probe_attempts=0;
        probe_phase=PROBE_READ_PACK;
        break;
    case PROBE_READ_PACK:
        if(endpoint_url("/config/pack",url) && fetch_json(url,song_response,sizeof(song_response))) {
            cJSON *root=pl_json_depth_ok(song_response)?cJSON_ParseWithOpts(song_response,NULL,true):NULL;
            if(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(root,"prefsAvailable"))) {
                const cJSON *esp32=cJSON_GetObjectItemCaseSensitive(root,"esp32Pack");
                if(cJSON_IsFalse(esp32)) advertised_options=0;
                modern_pack=cJSON_IsTrue(esp32);
                probe_phase=PROBE_COMPLETE;
            }
            cJSON_Delete(root);
        }
        if(++probe_attempts>=3) probe_phase=PROBE_COMPLETE;
        break;
    default: break;
    }
}
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) { connected = false; atomic_fetch_add(&connection_generation,1); pl_set_unlocked(false); }
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) connected = true;
}
static bool fetch_status(protonlink_status_t *out)
{
    unsigned generation=atomic_load(&connection_generation);
    if(!fetch_json(CONFIG_PROTONLINK_STATUS_URL,response,sizeof(response)) || !protonlink_decode(response,out) ||
       !connected || generation!=atomic_load(&connection_generation)) return false;
    if(probe_phase==PROBE_COMPLETE && capabilities_generation==generation) {
        out->advertised_options=advertised_options;
        out->modern_pack=modern_pack;
    }
    return true;
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
bool pl_request_track(int track)
{
    command_request_t req={.select_track=true,.target=track,.epoch=atomic_load(&lock_epoch)};
    if (!requests || !pl_is_unlocked() || !protonlink_snapshot(&req.before) || !pl_track_allowed(&req.before,track)) return false;
    return enqueue(&req);
}
bool pl_request_option(pl_option_t option)
{
    command_request_t req={.option=true,.option_id=option,.epoch=atomic_load(&lock_epoch)};
    if(!requests || !pl_is_unlocked() || !protonlink_snapshot(&req.before) ||
       !pl_option_path(option,&req.before)) return false;
    return enqueue(&req);
}

static bool send_path(command_request_t *req,const char *path)
{
    if (!path || !pl_is_unlocked() || req->epoch != atomic_load(&lock_epoch) || !connected) return false;
    char url[256];
    if(!endpoint_url(path,url)) return false;
    esp_http_client_config_t cfg = {.url=url,.timeout_ms=2500,.disable_auto_redirect=true,.method=HTTP_METHOD_PUT};
    esp_http_client_handle_t client=esp_http_client_init(&cfg);
    if (!client) return false;
    bool ok=false;
    if (pl_is_unlocked() && connected && req->epoch == atomic_load(&lock_epoch))
        ok=esp_http_client_perform(client)==ESP_OK && esp_http_client_get_status_code(client)==200;
    esp_http_client_cleanup(client);
    return ok;
}
static void publish(const protonlink_status_t *sample, unsigned generation)
{
    xSemaphoreTake(mutex,portMAX_DELAY);
    latest=*sample;
    received_us=esp_timer_get_time();
    received_generation=generation;
    xSemaphoreGive(mutex);
}
static void execute_request(command_request_t *req)
{
    protonlink_status_t fresh;
    unsigned generation=atomic_load(&connection_generation);
    if (!pl_is_unlocked() || req->epoch!=atomic_load(&lock_epoch) || !fetch_status(&fresh)) return;
    publish(&fresh,generation);
    if(req->option) {
        const char *before=pl_option_path(req->option_id,&req->before);
        const char *path=pl_option_path(req->option_id,&fresh);
        if(before && path && !strcmp(before,path)) send_path(req,path);
        return;
    }
    if (req->select_track) {
        char path[48];
        if (!pl_track_allowed(&fresh,req->target)) return;
        snprintf(path,sizeof(path),"/music/select?track=%d",req->target);
        send_path(req,path);
        return;
    }
    if (!req->volume) {
        const char *path=pl_command_path(req->action,&fresh);
        if ((req->action==PL_PLAY_STOP || req->action==PL_PAUSE_RESUME) &&
            (fresh.playing!=req->before.playing || fresh.paused!=req->before.paused)) return;
        send_path(req,path);
        return;
    }
    /* Each confirmed step changes the integer percentage toward the target. */
    for (int step=0;step<100;step++) {
        int prior=pl_volume_value(&fresh,req->channel);
        const char *path=pl_volume_step(&fresh,req->channel,req->target);
        if (!path || !send_path(req,path)) break;
        bool advanced=false;
        for (int attempt=0;attempt<4;attempt++) {
            vTaskDelay(pdMS_TO_TICKS(200));
            if (!pl_is_unlocked() || req->epoch!=atomic_load(&lock_epoch) || !connected) return;
            if (!fetch_status(&fresh)) { pl_set_unlocked(false); return; }
            publish(&fresh,generation);
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
            xSemaphoreTake(mutex,portMAX_DELAY);
            if(song_state==PL_SYNC_PENDING || song_state==PL_SYNC_READY) song_state=PL_SYNC_UNAVAILABLE;
            xSemaphoreGive(mutex);
            esp_wifi_connect();
        } else {
            command_request_t request;
            if (requests && xQueueReceive(requests,&request,0)==pdTRUE) { execute_request(&request); atomic_store(&busy,false); }
            protonlink_status_t sample;
            unsigned generation=atomic_load(&connection_generation);
            if (fetch_status(&sample)) {
                publish(&sample,generation);
                pl_sync_state_t track_sync=protonlink_song_list(NULL,0,NULL,NULL);
                if(track_sync==PL_SYNC_PENDING) sync_songs(&sample);
                else if(probe_phase!=PROBE_COMPLETE || capabilities_generation!=generation) probe_capabilities();
            } else {
                xSemaphoreTake(mutex, portMAX_DELAY);
                received_us = 0;
                if(song_state==PL_SYNC_PENDING || song_state==PL_SYNC_READY) song_state=PL_SYNC_UNAVAILABLE;
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
    bool fresh=connected && received_generation==atomic_load(&connection_generation) && received_us>0 && esp_timer_get_time()-received_us<6000000;
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
