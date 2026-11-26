#include "lvgl.h"
#include "control.h"
#ifndef PROTONLINK_DESKTOP
#include "sdkconfig.h"
#endif
#include "ui.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

static lv_obj_t *status_label, *telemetry_label, *lock_label;
static lv_obj_t *quick, *field, *monitor, *menu, *library, *clock_label, *field_clock, *monitor_clock;
static lv_obj_t *field_progress, *dot_progress, *pack_progress[3], *leds[3][20], *radar_dot[2], *monitor_state, *field_state, *library_track;
static lv_obj_t *dot_button, *monitor_dots;
static bool field_locked=true, unlock_armed, released, gesture_active, gesture_done;
static uint32_t gesture_start, armed_at;
static lv_point_t gesture_origin;
static lv_obj_t *gesture_target;
static int gesture_action;
static void refresh(lv_timer_t *timer);
static void show_page(lv_obj_t *page);
static void update_leds(int channel,int value);
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
    if (code==LV_EVENT_VALUE_CHANGED && dragging[i]) { int v=(int)lv_slider_get_value(levels[i]); lv_label_set_text_fmt(level_labels[i],"%d",v); update_leds(i,v); }
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
static void update_leds(int channel,int value)
{
    for(int j=0;j<20;j++) if(leds[channel][j])
        lv_obj_set_style_bg_color(leds[channel][j],lv_color_hex(value>j*5?channel_color[channel]:0x283638),0);
}
static void show_page(lv_obj_t *page)
{
    lv_obj_t *pages[]={quick,field,monitor,menu,library};
    for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++) {
        if(pages[i]==page) lv_obj_remove_flag(pages[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i],LV_OBJ_FLAG_HIDDEN);
    }
}
static void engage_lock(void)
{
    pl_set_unlocked(false); field_locked=true; unlock_armed=false; released=false;
    gesture_active=false; gesture_done=true;
    lv_bar_set_value(field_progress,0,LV_ANIM_OFF);
    lv_label_set_text(lock_label,"HOLD TO UNLOCK");
    show_page(field);
}
static void nav_event(lv_event_t *e)
{
    if(field_locked) return;
    show_page(lv_event_get_user_data(e));
}
static void point_now(lv_point_t *p)
{
    lv_indev_t *in=lv_indev_active();
    if(in) lv_indev_get_point(in,p); else *p=(lv_point_t){0,0};
}
enum { G_FIELD=-3, G_DOTS=-2, G_MONITOR=-1 };
static void gesture_event(lv_event_t *e)
{
    int action=(int)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t code=lv_event_get_code(e);
    lv_point_t point; point_now(&point);
    if(code==LV_EVENT_PRESSED) {
        if(action==G_FIELD && (!field_locked || !ui_online)) return;
        if(action!=G_FIELD && field_locked) return;
        if(action>=0 && (!pl_is_unlocked() || pl_busy())) return;
        gesture_active=true; gesture_done=false; gesture_action=action;
        gesture_target=lv_event_get_target(e); gesture_start=lv_tick_get(); gesture_origin=point;
    } else if(gesture_active && gesture_target==lv_event_get_target(e)) {
        int dx=point.x-gesture_origin.x,dy=point.y-gesture_origin.y;
        if(code==LV_EVENT_PRESSING && !(action==G_FIELD && unlock_armed && released) &&
           (dx*dx+dy*dy>18*18)) gesture_active=false;
        if(code==LV_EVENT_PRESS_LOST) { gesture_active=false; if(action==G_FIELD) engage_lock(); }
        if(code==LV_EVENT_RELEASED) {
            gesture_active=false;
            if(action==G_FIELD && unlock_armed) {
                if(released && dx < -45 && dy > -65 && dy < 65 && ui_online) {
                    field_locked=false; unlock_armed=false; released=false;
                    pl_set_unlocked(true); show_page(quick);
                } else released=true;
            } else if(!gesture_done && (action==G_DOTS || action==G_MONITOR))
                show_page(action==G_DOTS?monitor:quick);
        }
    }
}
static void gesture_tick(lv_timer_t *timer)
{
    (void)timer;
    if(unlock_armed && lv_tick_elaps(armed_at)>=5000) engage_lock();
    if(!gesture_active) {
        if(!unlock_armed) lv_bar_set_value(field_progress,0,LV_ANIM_OFF);
        lv_bar_set_value(dot_progress,0,LV_ANIM_OFF);
        for(int i=0;i<3;i++) lv_bar_set_value(pack_progress[i],0,LV_ANIM_OFF);
        return;
    }
    uint32_t elapsed=lv_tick_elaps(gesture_start);
    int a=gesture_action;
    unsigned threshold=a==G_FIELD?2000:a<0?1500:1000;
    int progress=elapsed>=threshold?100:(int)(elapsed*100/threshold);
    if(a==G_FIELD && !unlock_armed) lv_bar_set_value(field_progress,progress,LV_ANIM_OFF);
    else if(a==G_DOTS) lv_bar_set_value(dot_progress,progress,LV_ANIM_OFF);
    else if(a>=0) lv_bar_set_value(pack_progress[a],progress,LV_ANIM_OFF);
    if(gesture_done || elapsed<threshold) return;
    gesture_done=true;
    if(a==G_FIELD) {
        if(!unlock_armed && ui_online) { unlock_armed=true; released=false; armed_at=lv_tick_get(); lv_label_set_text(lock_label,"RELEASE, THEN SWIPE LEFT"); }
    } else if(a<0) engage_lock();
    else if(pl_is_unlocked() && !pl_busy()) {
        if(!pl_request((pl_action_t)a)) lv_label_set_text(status_label,"REQUEST BLOCKED");
        else lv_label_set_text(status_label,"REQUEST QUEUED");
    }
}
static void refresh(lv_timer_t *timer)
{
    (void)timer;
    protonlink_status_t s;
    ui_online = protonlink_snapshot(&s);
    if (!ui_online && !field_locked) engage_lock();
    bool armed = ui_online && pl_is_unlocked();
    bool busy=pl_busy();
    if(!field_locked && !armed && ui_online) {
#if defined(CONFIG_PROTONLINK_ENABLE_CONTROL) || defined(PROTONLINK_DESKTOP)
        engage_lock();
#endif
    }
    time_t now=time(NULL); struct tm *local=localtime(&now); char stamp[16]="--:--";
    if(local && local->tm_year>=120) strftime(stamp,sizeof(stamp),"%H:%M",local);
    lv_label_set_text(clock_label,stamp); lv_label_set_text(field_clock,stamp); lv_label_set_text(monitor_clock,stamp);
    for(int i=0;i<2;i++) { double a=lv_tick_get()/1000.0; lv_obj_set_pos(radar_dot[i],200+(int)(cos(a)*100),270+(int)(sin(a)*100)); lv_obj_set_style_bg_color(radar_dot[i],lv_color_hex(ui_online?0x83e5a4:0x526059),0); }
    lv_label_set_text(field_state,ui_online?"TRACKING EQUIPMENT":"TELEMETRY UNAVAILABLE");
    lv_label_set_text(monitor_state,ui_online?"TRACKING EQUIPMENT":"TELEMETRY UNAVAILABLE");
#if defined(CONFIG_PROTONLINK_ENABLE_CONTROL) || defined(PROTONLINK_DESKTOP)
    lv_label_set_text(status_label, armed ? (busy ? "SYNCING" : "Adjust volume") : "FIELD LOCK");
#else
    lv_label_set_text(status_label, "READ ONLY");
#endif
    if (ui_online) {
        lv_label_set_text(telemetry_label,s.firing?"FIRING":!strcmp(s.temperature,"Venting")?"VENT CYCLE":!strcmp(s.cyclotron,"Warning")?"WARNING":!strcmp(s.cyclotron,"Critical")?"CRITICAL":!strcmp(s.cyclotron,"Recovery")?"RECOVERY":s.pack_on?"SYSTEM ONLINE":"SYSTEM STANDBY");
        if(s.track>=500 && s.track_first>=500) lv_label_set_text_fmt(track_label,"%d   Track %d   >",s.track,s.track-s.track_first+1);
        else lv_label_set_text(track_label,"TRACK UNAVAILABLE");
        lv_label_set_text(library_track,lv_label_get_text(track_label));
        for(int i=0;i<4;i++) { uint32_t c=s.pack_on && i==(int)((lv_tick_get()/250)%4)?0xff5949:0x452622; lv_obj_set_style_bg_color(dots[i],lv_color_hex(c),0); lv_obj_set_style_bg_color(lv_obj_get_child(monitor_dots,i),lv_color_hex(c),0); }
        int v[] = {s.master,s.effects,s.music};
        for (int i=0;i<3;i++) {
            if (!dragging[i] || !armed || busy) {
                dragging[i]=false;
                lv_slider_set_value(levels[i],v[i],LV_ANIM_OFF);
                lv_label_set_text_fmt(level_labels[i],"%d",v[i]); update_leds(i,v[i]);
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
            lv_label_set_text(level_labels[i],"--"); update_leds(i,0);
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
    lv_obj_set_style_bg_color(b,lv_color_hex(0x151b1b),LV_STATE_DISABLED);
    lv_obj_set_style_bg_opa(b,LV_OPA_COVER,LV_STATE_DISABLED);
    lv_obj_set_style_text_color(b,lv_color_hex(0x64706d),LV_STATE_DISABLED);
    lv_obj_t *label=lv_label_create(b);
    lv_label_set_text(label,text);
    lv_obj_center(label);
    if (label_out) *label_out=label;
    return b;
}
static void action_button(lv_obj_t *parent,pl_action_t a,const char *text,int width,lv_obj_t **label)
{
    buttons[a]=make_button(parent,text,width,42,label);
    lv_obj_add_event_cb(buttons[a],a<=PL_VENT?gesture_event:command,a<=PL_VENT?LV_EVENT_ALL:LV_EVENT_CLICKED,(void *)(intptr_t)a);
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
static lv_obj_t *page_new(lv_obj_t *root,uint32_t bg)
{
    lv_obj_t *p=lv_obj_create(root); lv_obj_remove_style_all(p);
    lv_obj_set_size(p,410,502); lv_obj_center(p);
    lv_obj_set_style_bg_color(p,lv_color_hex(bg),0); lv_obj_set_style_bg_opa(p,LV_OPA_COVER,0);
    lv_obj_remove_flag(p,LV_OBJ_FLAG_SCROLLABLE); return p;
}
static lv_obj_t *label_at(lv_obj_t *parent,const char *str,int x,int y,uint32_t color)
{
    lv_obj_t *l=lv_label_create(parent); lv_label_set_text(l,str);
    lv_obj_set_style_text_color(l,lv_color_hex(color),0); lv_obj_set_pos(l,x,y); return l;
}
static lv_obj_t *progress_at(lv_obj_t *parent,int w,int y,uint32_t color)
{
    lv_obj_t *b=lv_bar_create(parent); lv_obj_set_size(b,w,3); lv_obj_align(b,LV_ALIGN_BOTTOM_MID,0,y);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x20312b),0);
    lv_obj_set_style_bg_color(b,lv_color_hex(color),LV_PART_INDICATOR);
    lv_obj_remove_flag(b,LV_OBJ_FLAG_CLICKABLE); return b;
}
static lv_obj_t *make_dots(lv_obj_t *parent)
{
    lv_obj_t *b=lv_obj_create(parent); lv_obj_remove_style_all(b); lv_obj_set_size(b,110,34);
    lv_obj_remove_flag(b,LV_OBJ_FLAG_SCROLLABLE); lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);
    for(int i=0;i<4;i++) {
        lv_obj_t *d=lv_obj_create(b); lv_obj_remove_style_all(d); lv_obj_set_size(d,15,15);
        lv_obj_set_pos(d,8+i*24,8); lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_opa(d,LV_OPA_COVER,0); lv_obj_set_style_bg_color(d,lv_color_hex(0x452622),0);
        lv_obj_set_style_border_width(d,2,0); lv_obj_set_style_border_color(d,lv_color_hex(0x626d6c),0);
        lv_obj_remove_flag(d,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    }
    return b;
}
static void radar(lv_obj_t *p,lv_obj_t **clock,lv_obj_t **state,int index)
{
    label_at(p,"EQUIPMENT STATUS SCAN",100,65,0x82b693);
    *clock=label_at(p,"--:--",155,93,0xb0f8c4); lv_obj_set_style_text_font(*clock,&lv_font_montserrat_24,0);
    for(int i=4;i>=1;i--) {
        lv_obj_t *r=lv_obj_create(p); lv_obj_remove_style_all(r);
        lv_obj_set_size(r,70*i,70*i); lv_obj_align(r,LV_ALIGN_CENTER,0,20);
        lv_obj_set_style_radius(r,LV_RADIUS_CIRCLE,0); lv_obj_set_style_border_width(r,1,0);
        lv_obj_set_style_border_color(r,lv_color_hex(0x315e3d),0);
        lv_obj_remove_flag(r,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    }
    for(int i=0;i<2;i++) {
        lv_obj_t *axis=lv_obj_create(p); lv_obj_remove_style_all(axis); lv_obj_set_size(axis,i?1:280,i?280:1);
        lv_obj_align(axis,LV_ALIGN_CENTER,0,20); lv_obj_set_style_bg_color(axis,lv_color_hex(0x315e3d),0);
        lv_obj_set_style_bg_opa(axis,LV_OPA_COVER,0); lv_obj_remove_flag(axis,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    }
    radar_dot[index]=lv_obj_create(p); lv_obj_remove_style_all(radar_dot[index]);
    lv_obj_set_size(radar_dot[index],10,10); lv_obj_set_style_radius(radar_dot[index],LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_opa(radar_dot[index],LV_OPA_COVER,0); lv_obj_set_style_bg_color(radar_dot[index],lv_color_hex(0x83e5a4),0);
    lv_obj_remove_flag(radar_dot[index],LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    *state=label_at(p,"TELEMETRY UNAVAILABLE",85,420,0x82b693);
}
void protonlink_ui_create(void)
{
    lv_obj_t *root=lv_screen_active();
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    quick=page_new(root,0x141a1b); field=page_new(root,0x020805); monitor=page_new(root,0x020805);
    menu=page_new(root,0x141a1b); library=page_new(root,0x141a1b);
    lv_obj_t *screen=quick;
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
    clock_label=lv_label_create(header);
    lv_obj_set_style_text_color(clock_label,lv_color_hex(0xdbe1db),0);
    lv_obj_t *status=line(screen,34);
    lv_obj_t *entry=make_button(status,"SYSTEM STANDBY >",252,30,&telemetry_label);
    lv_obj_add_event_cb(entry,nav_event,LV_EVENT_CLICKED,menu);
    lv_obj_set_style_text_color(telemetry_label,lv_color_hex(0xb3c8b8),0);
    dot_button=make_dots(status);
    for(int i=0;i<4;i++) dots[i]=lv_obj_get_child(dot_button,i);
    dot_progress=progress_at(dot_button,100,0,0xe6b75e);
    lv_obj_add_event_cb(dot_button,gesture_event,LV_EVENT_ALL,(void *)(intptr_t)G_DOTS);
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
        lv_obj_set_style_text_color(name,lv_color_hex(0xd2d6cd),0);
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
        lv_obj_set_style_bg_opa(levels[i],LV_OPA_TRANSP,LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(levels[i],lv_color_hex(0x8b9897),LV_PART_KNOB);
        lv_obj_set_style_bg_grad_color(levels[i],lv_color_hex(0x313a3a),LV_PART_KNOB);
        lv_obj_set_style_bg_grad_dir(levels[i],LV_GRAD_DIR_HOR,LV_PART_KNOB);
        lv_obj_set_style_border_width(levels[i],1,LV_PART_KNOB);
        lv_obj_set_style_border_color(levels[i],lv_color_hex(0x9ca9a8),LV_PART_KNOB);
        for(int j=0;j<20;j++) {
            leds[i][j]=lv_obj_create(panel); lv_obj_remove_style_all(leds[i][j]);
            lv_obj_set_size(leds[i][j],14,12); lv_obj_set_pos(leds[i][j],30+j*16,46);
            lv_obj_set_style_bg_opa(leds[i][j],LV_OPA_COVER,0);
            lv_obj_remove_flag(leds[i][j],LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        }
        lv_obj_move_foreground(levels[i]);
        lv_obj_set_style_bg_opa(levels[i],LV_OPA_TRANSP,LV_PART_MAIN);
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
    lv_obj_t *transport=line(screen,18);
    lv_obj_t *aux=lv_label_create(transport);
    lv_label_set_text(aux,"AUX TRANSPORT / CH-03");
    lv_obj_set_style_text_font(aux,&lv_font_montserrat_12,0);
    lv_obj_set_style_text_color(aux,lv_color_hex(0xa4b1a9),0);
    lv_obj_t *track_button=make_button(screen,"TRACK UNAVAILABLE",386,26,&track_label);
    lv_obj_add_event_cb(track_button,nav_event,LV_EVENT_CLICKED,library);
    lv_obj_set_size(track_label,370,22);
    lv_obj_set_style_bg_color(track_label,lv_color_hex(0x040a08),0);
    lv_obj_set_style_bg_opa(track_label,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(track_label,lv_color_hex(0x8bd89f),0);
    lv_obj_set_style_text_align(track_label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_t *music=line(screen,35);
    action_button(music,PL_PLAY_STOP,"START",86,&play_label);
    action_button(music,PL_PAUSE_RESUME,"PAUSE",86,&pause_label);
    action_button(music,PL_PREVIOUS,"|<",56,NULL);
    action_button(music,PL_NEXT,">|",56,NULL);
    lv_obj_t *list=make_button(music,"LIST",66,33,NULL); lv_obj_add_event_cb(list,nav_event,LV_EVENT_CLICKED,library);
    for(int i=PL_PLAY_STOP;i<=PL_NEXT;i++) lv_obj_set_height(buttons[i],33);
    lv_obj_t *hazard=line(screen,8);
    lv_obj_set_layout(hazard,LV_LAYOUT_NONE);
    for(int i=0;i<24;i++) {
        lv_obj_t *stripe=lv_obj_create(hazard);
        lv_obj_remove_style_all(stripe);
        lv_obj_set_size(stripe,10,14);
        lv_obj_set_style_transform_rotation(stripe,300,0);
        lv_obj_set_pos(stripe,i*16,-3);
        lv_obj_set_style_bg_color(stripe,lv_color_hex(0xd5a644),0);
        lv_obj_set_style_bg_opa(stripe,LV_OPA_COVER,0);
    }
    lv_obj_t *pack=line(screen,62);
    action_button(pack,PL_START,"START\nHOLD",122,NULL);
    action_button(pack,PL_STOP,"STOP\nHOLD",122,NULL);
    action_button(pack,PL_VENT,"VENT\nHOLD",122,NULL);
    for(int i=0;i<3;i++) {
        lv_obj_set_height(buttons[i],62);
        pack_progress[i]=progress_at(buttons[i],100,-2,switch_color[i]);
    }
    status_label=lv_label_create(screen); lv_obj_set_style_text_color(status_label,lv_color_hex(0xe6b75e),0);
    lv_obj_set_style_text_font(status_label,&lv_font_montserrat_12,0);
    radar(field,&field_clock,&field_state,0); radar(monitor,&monitor_clock,&monitor_state,1);
    label_at(field,"LINK / PL-01",16,15,0x82b693);
    lock_label=label_at(field,"HOLD TO UNLOCK",65,461,0xe6b75e);
    field_progress=progress_at(field,370,-8,0x85e7aa);
    lv_obj_add_flag(field,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(field,gesture_event,LV_EVENT_ALL,(void *)(intptr_t)G_FIELD);
    label_at(monitor,"PROTONLINK",20,17,0xd2d6cd);
    monitor_dots=make_dots(monitor); lv_obj_set_pos(monitor_dots,280,10);
    lv_obj_add_event_cb(monitor_dots,gesture_event,LV_EVENT_ALL,(void *)(intptr_t)G_MONITOR);
    label_at(monitor,"PKE MONITOR / TAP LIGHTS TO RETURN",35,465,0xe6b75e);
    label_at(menu,"COMMAND / SUBSYSTEMS",25,24,0xd2d6cd);
    lv_obj_t *back=make_button(menu,"FIELD CONTROL",350,62,NULL); lv_obj_set_pos(back,30,70); lv_obj_add_event_cb(back,nav_event,LV_EVENT_CLICKED,quick);
    lv_obj_t *pke=make_button(menu,"PKE MONITOR",350,62,NULL); lv_obj_set_pos(pke,30,145); lv_obj_add_event_cb(pke,nav_event,LV_EVENT_CLICKED,monitor);
    lv_obj_t *tracks=make_button(menu,"AUX TRACK DATABASE",350,62,NULL); lv_obj_set_pos(tracks,30,220); lv_obj_add_event_cb(tracks,nav_event,LV_EVENT_CLICKED,library);
    label_at(library,"AUX TRACK DATABASE",25,24,0xd2d6cd);
    lv_obj_t *return_button=make_button(library,"RETURN TO FIELD CONTROL",350,48,NULL); lv_obj_set_pos(return_button,30,65); lv_obj_add_event_cb(return_button,nav_event,LV_EVENT_CLICKED,quick);
    library_track=label_at(library,"TRACK UNAVAILABLE",30,145,0x8bd89f);
    label_at(library,"Track selection not available yet",30,190,0xa4b1a9);
    engage_lock();
    lv_timer_create(gesture_tick,20,NULL);
    refresh(NULL);
    lv_timer_create(refresh,250,NULL);
}
