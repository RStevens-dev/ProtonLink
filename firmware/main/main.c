#include "esp_err.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "control.h"
#include "sdkconfig.h"
#include <stdint.h>

static lv_obj_t *status_label, *telemetry_label, *lock_label;
static lv_obj_t *levels[3], *level_labels[3], *buttons[12];
static lv_obj_t *play_label, *pause_label;
static bool ui_online;
static void command(lv_event_t *event)
{
    pl_action_t a = (pl_action_t)(intptr_t)lv_event_get_user_data(event);
    if (!pl_request(a)) lv_label_set_text(status_label, "REQUEST BLOCKED / BUSY");
    else lv_label_set_text(status_label, "REQUEST QUEUED");
}
static void lock_click(lv_event_t *event)
{
    (void)event;
    pl_set_unlocked(false);
}
static void unlock_hold(lv_event_t *event)
{
    (void)event;
    if (ui_online && !pl_is_unlocked()) pl_set_unlocked(true);
}
static void refresh(lv_timer_t *timer)
{
    (void)timer;
    protonlink_status_t s;
    ui_online = protonlink_snapshot(&s);
    if (!ui_online) pl_set_unlocked(false);
    bool armed = ui_online && pl_is_unlocked();
    lv_label_set_text(lock_label, armed ? "TAP TO LOCK" : "HOLD TO UNLOCK");
#ifdef CONFIG_PROTONLINK_ENABLE_CONTROL
    lv_label_set_text(status_label, armed ? "CONTROL ENABLED" : "FIELD LOCK");
#else
    lv_label_set_text(status_label, "READ ONLY | FIELD LOCK");
#endif
    if (ui_online) {
        lv_label_set_text_fmt(telemetry_label, "%s | %s | %s\nTrack %d", s.pack_on ? "PACK ON" : "PACK OFF", s.firing ? "FIRING" : s.cyclotron, s.temperature, s.track);
        int v[] = {s.master,s.effects,s.music};
        for (int i=0;i<3;i++) {
            lv_bar_set_value(levels[i],v[i],LV_ANIM_OFF);
            lv_label_set_text_fmt(level_labels[i],"%d%%",v[i]);
        }
        lv_label_set_text(play_label,s.playing ? "STOP" : "PLAY");
        lv_label_set_text(pause_label,s.paused ? "RESUME" : "PAUSE");
    } else {
        lv_label_set_text(telemetry_label,"GPStar offline / stale");
        for (int i=0;i<3;i++) {
            lv_bar_set_value(levels[i],0,LV_ANIM_OFF);
            lv_label_set_text(level_labels[i],"--");
        }
        lv_label_set_text(play_label,"PLAY");
        lv_label_set_text(pause_label,"PAUSE");
    }
    for (int i=0;i<12;i++) {
        if (armed && pl_command_path((pl_action_t)i,&s)) lv_obj_remove_state(buttons[i],LV_STATE_DISABLED);
        else lv_obj_add_state(buttons[i],LV_STATE_DISABLED);
    }
}
static lv_obj_t *make_button(lv_obj_t *parent,const char *text,int width,int height,lv_obj_t **label_out)
{
    lv_obj_t *b=lv_button_create(parent);
    lv_obj_set_size(b,width,height);
    lv_obj_t *label=lv_label_create(b);
    lv_label_set_text(label,text);
    lv_obj_center(label);
    if (label_out) *label_out=label;
    return b;
}
static void action_button(lv_obj_t *parent,pl_action_t a,const char *text,int width,lv_obj_t **label)
{
    buttons[a]=make_button(parent,text,width,42,label);
    lv_obj_add_event_cb(buttons[a],command,a<=PL_VENT ? LV_EVENT_LONG_PRESSED : LV_EVENT_CLICKED,(void *)(intptr_t)a);
    lv_obj_add_state(buttons[a],LV_STATE_DISABLED);
}
static lv_obj_t *line(lv_obj_t *screen,int height)
{
    lv_obj_t *r=lv_obj_create(screen);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r,390,height);
    lv_obj_set_flex_flow(r,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r,LV_FLEX_ALIGN_SPACE_EVENLY,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    return r;
}
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
    lv_obj_t *screen=lv_screen_active();
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x101418),0);
    lv_obj_set_style_text_color(screen,lv_color_hex(0xE5E7EB),0);
    lv_obj_set_style_pad_row(screen,5,0);
    lv_obj_set_flex_flow(screen,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_t *title=lv_label_create(screen);
    lv_label_set_text(title,"PROTONLINK");
    lv_obj_set_style_text_font(title,&lv_font_montserrat_24,0);
    status_label=lv_label_create(screen);
    lv_obj_set_style_text_color(status_label,lv_color_hex(0xF0B44C),0);
    telemetry_label=lv_label_create(screen);
    lv_obj_set_style_text_align(telemetry_label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_t *lock=make_button(screen,"HOLD TO UNLOCK",280,42,&lock_label);
    lv_obj_add_event_cb(lock,unlock_hold,LV_EVENT_LONG_PRESSED,NULL);
    lv_obj_add_event_cb(lock,lock_click,LV_EVENT_SHORT_CLICKED,NULL);
    const char *names[]={"MASTER","EFFECTS","MUSIC"};
    for(int i=0;i<3;i++) {
        lv_obj_t *r=line(screen,43);
        lv_obj_t *name=lv_label_create(r);
        lv_obj_set_width(name,68);
        lv_label_set_text(name,names[i]);
        action_button(r,(pl_action_t)(PL_MASTER_DOWN+i*2),"-",43,NULL);
        levels[i]=lv_bar_create(r);
        lv_obj_set_size(levels[i],145,16);
        lv_bar_set_range(levels[i],0,100);
        level_labels[i]=lv_label_create(r);
        lv_obj_set_width(level_labels[i],40);
        action_button(r,(pl_action_t)(PL_MASTER_UP+i*2),"+",43,NULL);
    }
    lv_obj_t *music=line(screen,45);
    action_button(music,PL_PREVIOUS,"PREV",78,NULL);
    action_button(music,PL_PLAY_STOP,"PLAY",92,&play_label);
    action_button(music,PL_PAUSE_RESUME,"PAUSE",92,&pause_label);
    action_button(music,PL_NEXT,"NEXT",78,NULL);
    lv_obj_t *pack=line(screen,48);
    action_button(pack,PL_START,"START",112,NULL);
    action_button(pack,PL_STOP,"STOP",112,NULL);
    action_button(pack,PL_VENT,"VENT",112,NULL);
    lv_obj_t *hint=lv_label_create(screen);
    lv_label_set_text(hint,"HOLD PACK BUTTONS");
    refresh(NULL);
    lv_timer_create(refresh,500,NULL);
    bsp_display_unlock();
    protonlink_network_start();
}
