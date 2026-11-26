#include "esp_err.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "control.h"
#include "sdkconfig.h"
#include <stdint.h>
#include <string.h>

static lv_obj_t *status_label, *telemetry_label, *lock_label;
static lv_obj_t *levels[3], *level_labels[3], *buttons[12];
static lv_obj_t *play_label, *pause_label, *track_label, *dots[4];
static const uint32_t channel_color[]={0xe6b75e,0x78cfff,0x8bd89f};
static const uint32_t switch_color[]={0x8bd89f,0xe86b57,0x78cfff};
static bool ui_online;
static bool dragging[3];
static void slider_event(lv_event_t *event)
{
    int i=(int)(intptr_t)lv_event_get_user_data(event);
    lv_event_code_t code=lv_event_get_code(event);
    if (code==LV_EVENT_PRESSED) dragging[i]=true;
    if (code==LV_EVENT_VALUE_CHANGED && dragging[i]) lv_label_set_text_fmt(level_labels[i],"%d%%",(int)lv_slider_get_value(levels[i]));
    if (code==LV_EVENT_PRESS_LOST) dragging[i]=false;
    if (code==LV_EVENT_RELEASED && dragging[i]) {
        dragging[i]=false;
        int target=(int)lv_slider_get_value(levels[i]);
        if (ui_online && pl_is_unlocked() && !pl_busy()) pl_request_volume(i,target);
    }
}
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
    bool busy=pl_busy();
    lv_label_set_text(lock_label, armed ? "TAP TO LOCK" : "HOLD TO UNLOCK");
#ifdef CONFIG_PROTONLINK_ENABLE_CONTROL
    lv_label_set_text(status_label, armed ? (busy ? "SYNCING" : "UNLOCKED") : "FIELD LOCK");
#else
    lv_label_set_text(status_label, "READ ONLY");
#endif
    if (ui_online) {
        lv_label_set_text(telemetry_label,s.firing?"FIRING":!strcmp(s.temperature,"Venting")?"VENT CYCLE":!strcmp(s.cyclotron,"Warning")?"WARNING":!strcmp(s.cyclotron,"Critical")?"CRITICAL":!strcmp(s.cyclotron,"Recovery")?"RECOVERY":s.pack_on?"SYSTEM ONLINE":"SYSTEM STANDBY");
        lv_label_set_text_fmt(track_label,"%d  /  TRACK",s.track);
        for(int i=0;i<4;i++) lv_obj_set_style_bg_color(dots[i],lv_color_hex(s.pack_on?0xff5949:0x452622),0);
        int v[] = {s.master,s.effects,s.music};
        for (int i=0;i<3;i++) {
            if (!dragging[i] || !armed || busy) {
                dragging[i]=false;
                lv_slider_set_value(levels[i],v[i],LV_ANIM_OFF);
                lv_label_set_text_fmt(level_labels[i],"%d%%",v[i]);
            }
        }
        lv_label_set_text(play_label,s.playing ? "STOP" : "START");
        lv_obj_set_style_text_color(buttons[PL_PLAY_STOP],lv_color_hex(s.playing?0xe86b57:0x8bd89f),0);
        lv_label_set_text(pause_label,s.paused ? "RESUME" : "PAUSE");
    } else {
        lv_label_set_text(telemetry_label,"LINK LOST");
        lv_label_set_text(track_label,"---  /  NO LINK");
        for(int i=0;i<4;i++) lv_obj_set_style_bg_color(dots[i],lv_color_hex(0x452622),0);
        for (int i=0;i<3;i++) {
            dragging[i]=false;
            lv_slider_set_value(levels[i],0,LV_ANIM_OFF);
            lv_label_set_text(level_labels[i],"--");
        }
        lv_label_set_text(play_label,"START");
        lv_label_set_text(pause_label,"PAUSE");
    }
    for (int i=0;i<3;i++) {
        if (armed && !busy) lv_obj_remove_state(levels[i],LV_STATE_DISABLED);
        else lv_obj_add_state(levels[i],LV_STATE_DISABLED);
    }
    for (int i=0;i<12;i++) {
        if (!buttons[i]) continue;
        if (armed && !busy && pl_command_path((pl_action_t)i,&s)) lv_obj_remove_state(buttons[i],LV_STATE_DISABLED);
        else lv_obj_add_state(buttons[i],LV_STATE_DISABLED);
    }
}
static lv_obj_t *make_button(lv_obj_t *parent,const char *text,int width,int height,lv_obj_t **label_out)
{
    lv_obj_t *b=lv_button_create(parent);
    lv_obj_set_size(b,width,height);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x202829),0);
    lv_obj_set_style_border_color(b,lv_color_hex(0x626d6c),0);
    lv_obj_set_style_border_width(b,1,0);
    lv_obj_set_style_radius(b,4,0);
    lv_obj_set_style_shadow_width(b,0,0);
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
    lv_obj_set_style_text_color(buttons[a],lv_color_hex(a<=PL_VENT?switch_color[a]:a==PL_PLAY_STOP?0x8bd89f:a==PL_PAUSE_RESUME?0xe6b75e:0x78cfff),0);
}
static lv_obj_t *line(lv_obj_t *screen,int height)
{
    lv_obj_t *r=lv_obj_create(screen);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r,386,height);
    lv_obj_remove_flag(r,LV_OBJ_FLAG_SCROLLABLE);
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
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x141a1b),0);
    lv_obj_set_style_text_color(screen,lv_color_hex(0xd2d6cd),0);
    lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_set_style_pad_row(screen,4,0);
    lv_obj_set_flex_flow(screen,LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(screen,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_t *header=line(screen,30);
    lv_obj_t *title=lv_label_create(header);
    lv_label_set_recolor(title,true);
    lv_label_set_text(title,"PROTON#e86b57 LINK#");
    lv_obj_set_style_text_font(title,&lv_font_montserrat_24,0);
    status_label=lv_label_create(header);
    lv_obj_set_style_text_color(status_label,lv_color_hex(0xe6b75e),0);
    lv_obj_t *status=line(screen,34);
    telemetry_label=lv_label_create(status);
    lv_obj_set_style_text_color(telemetry_label,lv_color_hex(0xdbe1db),0);
    lv_obj_t *lock=make_button(status,"HOLD TO UNLOCK",150,32,&lock_label);
    lv_obj_add_event_cb(lock,unlock_hold,LV_EVENT_LONG_PRESSED,NULL);
    lv_obj_add_event_cb(lock,lock_click,LV_EVENT_SHORT_CLICKED,NULL);
    for(int i=0;i<4;i++) {
        dots[i]=lv_obj_create(status);
        lv_obj_remove_style_all(dots[i]);
        lv_obj_set_size(dots[i],12,12);
        lv_obj_set_style_radius(dots[i],LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_opa(dots[i],LV_OPA_COVER,0);
        lv_obj_set_style_border_width(dots[i],1,0);
        lv_obj_set_style_border_color(dots[i],lv_color_hex(0x626d6c),0);
    }
    const char *names[]={"SYSTEM OUTPUT","DISCHARGE EFFECTS","AUXILIARY AUDIO"};
    const char *codes[]={"CH-01 / MAIN","CH-02 / FX","CH-03 / MUSIC"};
    for(int i=0;i<3;i++) {
        lv_obj_t *panel=lv_obj_create(screen);
        lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(panel,386,76);
        lv_obj_set_style_pad_all(panel,0,0);
        lv_obj_set_style_bg_color(panel,lv_color_hex(0x1b2223),0);
        lv_obj_set_style_bg_grad_color(panel,lv_color_hex(0x353f40),0);
        lv_obj_set_style_bg_grad_dir(panel,LV_GRAD_DIR_VER,0);
        lv_obj_set_style_border_color(panel,lv_color_hex(0x64706d),0);
        lv_obj_set_style_border_width(panel,1,0);
        lv_obj_set_style_radius(panel,5,0);
        lv_obj_t *name=lv_label_create(panel);
        lv_label_set_text(name,names[i]);
        lv_obj_set_pos(name,18,4);
        lv_obj_t *code=lv_label_create(panel);
        lv_label_set_text(code,codes[i]);
        lv_obj_set_style_text_font(code,&lv_font_montserrat_12,0);
        lv_obj_set_style_text_color(code,lv_color_hex(0x8e9a98),0);
        lv_obj_set_pos(code,18,22);
        level_labels[i]=lv_label_create(panel);
        lv_obj_set_size(level_labels[i],66,29);
        lv_obj_align(level_labels[i],LV_ALIGN_TOP_RIGHT,-12,3);
        lv_obj_set_style_bg_color(level_labels[i],lv_color_hex(0x050909),0);
        lv_obj_set_style_bg_opa(level_labels[i],LV_OPA_COVER,0);
        lv_obj_set_style_border_width(level_labels[i],1,0);
        lv_obj_set_style_border_color(level_labels[i],lv_color_hex(0x52605e),0);
        lv_obj_set_style_text_align(level_labels[i],LV_TEXT_ALIGN_CENTER,0);
        lv_obj_set_style_text_font(level_labels[i],&lv_font_montserrat_24,0);
        lv_obj_set_style_text_color(level_labels[i],lv_color_hex(channel_color[i]),0);
        levels[i]=lv_slider_create(panel);
        lv_obj_set_size(levels[i],330,20);
        lv_obj_align(levels[i],LV_ALIGN_BOTTOM_MID,0,-12);
        lv_slider_set_range(levels[i],0,100);
        lv_obj_set_style_bg_color(levels[i],lv_color_hex(0x070c0e),LV_PART_MAIN);
        lv_obj_set_style_radius(levels[i],3,LV_PART_MAIN);
        lv_obj_set_style_border_width(levels[i],1,LV_PART_MAIN);
        lv_obj_set_style_border_color(levels[i],lv_color_hex(0x526266),LV_PART_MAIN);
        lv_obj_set_style_bg_color(levels[i],lv_color_hex(channel_color[i]),LV_PART_INDICATOR);
        lv_obj_set_style_radius(levels[i],2,LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(levels[i],lv_color_hex(0x8b9897),LV_PART_KNOB);
        lv_obj_set_style_radius(levels[i],3,LV_PART_KNOB);
        lv_obj_set_style_pad_left(levels[i],0,LV_PART_KNOB);
        lv_obj_set_style_pad_right(levels[i],0,LV_PART_KNOB);
        lv_obj_set_style_pad_top(levels[i],5,LV_PART_KNOB);
        lv_obj_set_style_pad_bottom(levels[i],5,LV_PART_KNOB);
        lv_obj_add_event_cb(levels[i],slider_event,LV_EVENT_ALL,(void *)(intptr_t)i);
        lv_obj_add_state(levels[i],LV_STATE_DISABLED);
        for(int tick=0;tick<=10;tick++) {
            lv_obj_t *mark=lv_obj_create(panel);
            lv_obj_remove_style_all(mark);
            lv_obj_set_size(mark,1,4);
            lv_obj_set_pos(mark,27+33*tick,69);
            lv_obj_set_style_bg_color(mark,lv_color_hex(0x78837d),0);
            lv_obj_set_style_bg_opa(mark,LV_OPA_COVER,0);
        }
    }
    lv_obj_t *transport=line(screen,20);
    lv_obj_t *aux=lv_label_create(transport);
    lv_label_set_text(aux,"AUX TRANSPORT / CH-03");
    lv_obj_set_style_text_font(aux,&lv_font_montserrat_12,0);
    lv_obj_set_style_text_color(aux,lv_color_hex(0xa4b1a9),0);
    track_label=lv_label_create(screen);
    lv_obj_set_size(track_label,386,26);
    lv_obj_set_style_bg_color(track_label,lv_color_hex(0x040a08),0);
    lv_obj_set_style_bg_opa(track_label,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(track_label,lv_color_hex(0x8bd89f),0);
    lv_obj_set_style_text_align(track_label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_t *music=line(screen,35);
    action_button(music,PL_PLAY_STOP,"START",92,&play_label);
    action_button(music,PL_PAUSE_RESUME,"PAUSE",92,&pause_label);
    action_button(music,PL_PREVIOUS,"PREV",78,NULL);
    action_button(music,PL_NEXT,"NEXT",78,NULL);
    for(int i=PL_PLAY_STOP;i<=PL_NEXT;i++) lv_obj_set_height(buttons[i],33);
    lv_obj_t *hazard=line(screen,8);
    lv_obj_set_layout(hazard,LV_LAYOUT_NONE);
    for(int i=0;i<24;i++) {
        lv_obj_t *stripe=lv_obj_create(hazard);
        lv_obj_remove_style_all(stripe);
        lv_obj_set_size(stripe,8,8);
        lv_obj_set_pos(stripe,i*16,0);
        lv_obj_set_style_bg_color(stripe,lv_color_hex(0xd5a644),0);
        lv_obj_set_style_bg_opa(stripe,LV_OPA_COVER,0);
    }
    lv_obj_t *pack=line(screen,54);
    action_button(pack,PL_START,"START\nHOLD",122,NULL);
    action_button(pack,PL_STOP,"STOP\nHOLD",122,NULL);
    action_button(pack,PL_VENT,"VENT\nHOLD",122,NULL);
    for(int i=0;i<3;i++) lv_obj_set_height(buttons[i],54);
    refresh(NULL);
    lv_timer_create(refresh,500,NULL);
    bsp_display_unlock();
    protonlink_network_start();
}
