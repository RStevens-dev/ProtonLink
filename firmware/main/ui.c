#include "lvgl.h"
#include "control.h"
#include "catalog.h"
#include "faces.h"
#include "options.h"
#include <stdio.h>
#ifndef PROTONLINK_DESKTOP
#include "sdkconfig.h"
#include "esp_attr.h"
#include "nvs.h"
#define PL_EXTERNAL_BSS EXT_RAM_BSS_ATTR
#else
#define PL_EXTERNAL_BSS
#endif
#include "ui.h"
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

static lv_obj_t *status_label, *telemetry_label, *lock_label;
static lv_obj_t *quick, *field, *monitor, *menu, *library, *clock_label, *field_face, *monitor_face, *diagnostics_art, *music_art, *display_page;
static pl_face_t selected_face=PL_FACE_PKE;
static bool dynamic_faces, contact_labels=true, horizontal_gestures=true;
static int marquee_palette;
static const uint32_t marquee_colors[]={0xefba69,0x8bd89f,0x78cfff,0xe86b57};
static const char *marquee_names[]={"AMBER","GREEN","BLUE","RED"};
static lv_obj_t *field_progress, *dot_progress, *pack_progress[3], *leds[3][20], *library_track;
static lv_obj_t *dot_button, *monitor_dots, *monitor_progress, *library_rows[7], *library_names[7], *library_prev, *library_next, *transport_light;
static lv_obj_t *pack_info, *music_page, *mixer, *diagnostics, *readiness, *settings, *audio_presets, *field_layout;
static lv_obj_t *music_summary, *music_state, *menu_summary, *menu_indicator;
static lv_obj_t *pack_values[4], *diagnostic_values[9], *readiness_values[5];

enum {
    MENU_BG=0x030607, MENU_BAR=0x0d161a, MENU_CARD=0x0e171a, MENU_EDGE=0x334348,
    MENU_TEXT=0xe0ecec, MENU_MUTED=0x91a6ac, MENU_ACTIVE=0x9fe2b4, MENU_AMBER=0xf5c06a,
    MENU_CYAN=0x82c8db, MENU_ALERT=0xff877b
};
static lv_obj_t *music_play, *music_pause, *preset_buttons[5], *reduction_label, *main_screen_label;
static lv_obj_t *library_return, *library_filter, *library_favorite, *library_empty;
static char track_readout[PL_TITLE_SIZE+24];
static lv_obj_t *quick_modules[4], *module_labels[4], *module_move[4], *module_toggle[4];
static const char *module_names[]={"MASTER","EFFECTS","MUSIC","TRANSPORT"};
static uint8_t module_order[]={0,1,2,3};
static bool module_hidden[4];
static lv_obj_t *library_origin;
static lv_obj_t *active_page;
static lv_obj_t *aux_buttons[PL_NEXT+1], *manual_vent_progress, *gesture_progress;
static lv_style_t disabled_style;
static lv_color_filter_dsc_t disabled_filter;
static bool reduced_motion, command_home, audio_reduced, favorites_only;
enum { EDIT_SEARCH, EDIT_TITLE, EDIT_IMPORT, EDIT_PLAYLIST, EDIT_PROFILE };
static lv_obj_t *effects_page, *era_page, *stream_page, *option_buttons[PL_OPT_COUNT];
static lv_obj_t *catalog_tools, *catalog_editor, *playlist_page, *profile_page;
static lv_obj_t *edit_prompt, *edit_value, *edit_ids, *edit_keyboard, *catalog_notice, *profile_label;
static lv_obj_t *playlist_buttons[PL_PLAYLIST_LIMIT];
static int edit_kind, edit_slot, edit_track;
static bool sync_waiting;
static char library_search[PL_TITLE_SIZE];
static int library_playlist=-1;

static void update_song_sync(const protonlink_status_t *s);
static int previous_audio[3];
static int visible_tracks[7], next_track_offset;
static bool library_has_previous, library_dirty;
static void update_subsystems(const protonlink_status_t *s, bool armed, bool busy);
static uint32_t page_entered;
static int library_offset, library_first=-1, library_last=-1, library_current=-1;
static uint32_t volume_due[3];
static int volume_target[3];
static bool volume_pending[3];
static uint32_t notice_at;
static const char *notice;
static void update_library(void);
static bool field_locked=true, unlock_armed, released, gesture_active, gesture_done;
static uint32_t gesture_start, armed_at;
static lv_point_t gesture_origin;
static lv_obj_t *gesture_target;
static int gesture_action;
static void refresh(lv_timer_t *timer);
static void show_page(lv_obj_t *page);
static void update_leds(int channel,int value);
static void save_preferences(void);
static lv_obj_t *levels[6], *level_labels[6], *buttons[PL_NEXT+1];
static lv_obj_t *play_label, *pause_label, *track_label, *dots[4];
static const uint32_t channel_color[]={0xe6b75e,0x78cfff,0x8bd89f};
static const uint32_t switch_color[]={0x8bd89f,0xe86b57,0x78cfff};
static bool ui_online;
static bool dragging[3];
static void slider_event(lv_event_t *event)
{
    int i=(int)(intptr_t)lv_event_get_user_data(event);
    lv_event_code_t code=lv_event_get_code(event);
    int channel=i%3;
    if (code==LV_EVENT_PRESSED) {
        page_entered=lv_tick_get();
        dragging[channel]=true;
    }
    if (code==LV_EVENT_VALUE_CHANGED && dragging[channel]) { int v=(int)lv_slider_get_value(levels[i]); lv_label_set_text_fmt(level_labels[i],"%d",v); if(i<3) update_leds(i,v); }
    if (code==LV_EVENT_PRESS_LOST) dragging[channel]=false;
    if (code==LV_EVENT_RELEASED && dragging[channel]) {
        dragging[channel]=false;
        int target=(int)lv_slider_get_value(levels[i]);
        if (!field_locked && ui_online && pl_is_unlocked()) { volume_target[channel]=target; volume_pending[channel]=true; volume_due[channel]=lv_tick_get(); }
    }
}
static void command(lv_event_t *event)
{
    page_entered=lv_tick_get();
    pl_action_t a = (pl_action_t)(intptr_t)lv_event_get_user_data(event);
    notice=pl_request(a)?"REQUEST QUEUED":"REQUEST BLOCKED / BUSY"; notice_at=lv_tick_get();
}
static void update_leds(int channel,int value)
{
    for(int j=0;j<20;j++) if(leds[channel][j])
        lv_obj_set_style_bg_color(leds[channel][j],lv_color_hex(value>j*5?channel_color[channel]:0x283638),0);
}
static void show_page(lv_obj_t *page)
{
    active_page=page; page_entered=lv_tick_get();
    lv_obj_t *pages[]={quick,field,monitor,menu,library,pack_info,music_page,mixer,diagnostics,readiness,settings,audio_presets,field_layout,display_page,effects_page,era_page,stream_page,catalog_tools,catalog_editor,playlist_page,profile_page};
    for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);i++) {
        if(pages[i]==page) lv_obj_remove_flag(pages[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i],LV_OBJ_FLAG_HIDDEN);
    }
}
static void engage_lock(void)
{
    pl_set_unlocked(false); field_locked=true; unlock_armed=false; released=false;
    gesture_active=false; gesture_done=true;
    audio_reduced=false;
    lv_label_set_text(reduction_label,"TEMPORARY 50% AUDIO REDUCTION");
    for(int i=0;i<3;i++) { volume_pending[i]=false; dragging[i]=false; }
    lv_bar_set_value(field_progress,0,LV_ANIM_OFF);
    lv_label_set_text(lock_label,"HOLD TO UNLOCK");
    show_page(field);
}
static void nav_event(lv_event_t *e)
{
    if(field_locked) return;
    lv_obj_t *dest=lv_event_get_user_data(e);
    if(active_page==settings && dest==quick) {
        command_home=false;
        save_preferences();
        lv_label_set_text(main_screen_label,"MAIN SCREEN: INDUSTRIAL CONTROL");
    }
    if(dest==library) {
        if(active_page==quick || active_page==music_page) {
            library_origin=active_page;
            favorites_only=false;
            library_playlist=-1;
            library_search[0]=0;
            library_offset=library_current>=library_first?(library_current-library_first)/7*7:0;
        }
        update_library();
    }
    show_page(dest);
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
        page_entered=lv_tick_get();
        if(gesture_active) return;
        if(action==G_FIELD && (!field_locked || !ui_online)) return;
        if(action!=G_FIELD && field_locked) return;
        if(action>=0 && (!pl_is_unlocked() || pl_busy())) return;
        gesture_active=true; gesture_done=false; gesture_action=action;
        gesture_target=lv_event_get_target(e);
        if(action>=0) gesture_progress=gesture_target==aux_buttons[PL_VENT]?manual_vent_progress:pack_progress[action];
        gesture_start=lv_tick_get(); gesture_origin=point;
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
                    pl_set_unlocked(true); show_page(command_home?menu:quick);
                } else released=true;
            } else if(!gesture_done && (action==G_DOTS || action==G_MONITOR))
                show_page(action==G_DOTS?monitor:quick);
        }
    }
}
static void gesture_tick(lv_timer_t *timer)
{
    (void)timer;
    if(!field_locked && active_page!=quick && active_page!=monitor && lv_tick_elaps(page_entered)>=30000) {
        if(command_home) engage_lock();
        else show_page(quick);
    }
    if(unlock_armed && lv_tick_elaps(armed_at)>=5000) engage_lock();
    if(!gesture_active) {
        if(!unlock_armed) lv_bar_set_value(field_progress,0,LV_ANIM_OFF);
        lv_bar_set_value(dot_progress,0,LV_ANIM_OFF);
        lv_bar_set_value(monitor_progress,0,LV_ANIM_OFF);
        for(int i=0;i<3;i++) lv_bar_set_value(pack_progress[i],0,LV_ANIM_OFF);
        lv_bar_set_value(manual_vent_progress,0,LV_ANIM_OFF);
        return;
    }
    uint32_t elapsed=lv_tick_elaps(gesture_start);
    int a=gesture_action;
    unsigned threshold=a==G_FIELD?2000:a<0?1500:1000;
    int progress=elapsed>=threshold?100:(int)(elapsed*100/threshold);
    if(a==G_FIELD && !unlock_armed) lv_bar_set_value(field_progress,progress,LV_ANIM_OFF);
    else if(a==G_DOTS) lv_bar_set_value(dot_progress,progress,LV_ANIM_OFF);
    else if(a==G_MONITOR) lv_bar_set_value(monitor_progress,progress,LV_ANIM_OFF);
    else if(a>=0) lv_bar_set_value(gesture_progress,progress,LV_ANIM_OFF);
    if(a>=0 && (!pl_is_unlocked() || pl_busy() || lv_obj_has_state(gesture_target,LV_STATE_DISABLED))) {
        gesture_active=false;
        gesture_target=NULL;
        return;
    }
    if(gesture_done || elapsed<threshold) return;
    gesture_done=true;
    if(a==G_FIELD) {
        if(!unlock_armed && ui_online) { unlock_armed=true; released=false; armed_at=lv_tick_get(); lv_label_set_text(lock_label,"RELEASE, THEN SWIPE LEFT"); }
    } else if(a<0) engage_lock();
    else {
        /* A successful request can disable its own button before LVGL sends
         * RELEASED. Retire the gesture first; never depend on that event. */
        gesture_active=false;
        gesture_target=NULL;
        bool accepted=pl_is_unlocked() && !pl_busy() && pl_request((pl_action_t)a);
        notice=accepted?"REQUEST QUEUED":"REQUEST BLOCKED";
        notice_at=lv_tick_get();
    }
}
static void refresh(lv_timer_t *timer)
{
    (void)timer;
    protonlink_status_t s={0};
    ui_online = protonlink_snapshot(&s);
    if (!ui_online && (!field_locked || unlock_armed || gesture_active)) engage_lock();
    update_song_sync(&s);
    bool armed = ui_online && pl_is_unlocked();
    bool busy=pl_busy();
    for(int i=0;i<3;i++) if(volume_pending[i] && !dragging[i] && lv_tick_elaps(volume_due[i])>=600 && armed && !busy) {
        volume_pending[i]=false; notice=pl_request_volume(i,volume_target[i])?"VOLUME REQUEST QUEUED":"VOLUME UNCHANGED / BLOCKED"; notice_at=lv_tick_get(); busy=pl_busy(); break;
    }
    if(!field_locked && !armed && ui_online) {
#if defined(CONFIG_PROTONLINK_ENABLE_CONTROL) || defined(PROTONLINK_DESKTOP)
        engage_lock();
#endif
    }
    time_t now=time(NULL); struct tm *local=localtime(&now); char stamp[16]="--:--", date[24]="";
    if(local && local->tm_year>=120) {
        strftime(stamp,sizeof(stamp),"%H:%M",local);
        strftime(date,sizeof(date),"%a %b %d",local);
    }
    lv_label_set_text(clock_label,stamp);
    if(active_page==field) {
        pl_face_t style=selected_face;
        if(dynamic_faces && ui_online) {
            if(!strcmp(s.cyclotron,"Critical") || !strcmp(s.cyclotron,"Warning")) style=PL_FACE_CONTAINMENT;
            else if(!strcmp(s.temperature,"Venting")) style=PL_FACE_CELL;
            else if(s.firing) style=PL_FACE_CYCLOTRON;
        }
        pl_face_update(field_face,style,&s,stamp,date,reduced_motion);
    }
    if(active_page==monitor) pl_face_update(monitor_face,PL_FACE_PKE,&s,stamp,date,reduced_motion);
    if(active_page==diagnostics) pl_face_update(diagnostics_art,PL_ART_DIAGNOSTICS,&s,stamp,date,reduced_motion);
    if(active_page==music_page) pl_face_update(music_art,PL_ART_TRANSPORT,&s,stamp,date,reduced_motion);
#if defined(CONFIG_PROTONLINK_ENABLE_CONTROL) || defined(PROTONLINK_DESKTOP)
    lv_label_set_text(status_label, armed ? (busy ? "SYNCING" : notice && lv_tick_elaps(notice_at)<2500 ? notice : "Adjust volume / 0.6s idle") : "FIELD LOCK");
#else
    lv_label_set_text(status_label, "READ ONLY");
#endif
    if (ui_online) {
        lv_label_set_text(telemetry_label,s.firing?"FIRING":!strcmp(s.temperature,"Venting")?"VENT CYCLE":!strcmp(s.cyclotron,"Warning")?"WARNING":!strcmp(s.cyclotron,"Critical")?"CRITICAL":!strcmp(s.cyclotron,"Recovery")?"RECOVERY":s.pack_on?"SYSTEM ONLINE":"SYSTEM STANDBY");
        char fallback[24];
        if(pl_catalog_range(s.track_first,s.track_last) && s.track>=s.track_first && s.track<=s.track_last)
            snprintf(track_readout,sizeof(track_readout),"%d   %s",s.track,pl_catalog_title(s.track,s.track_first,fallback));
        else strcpy(track_readout,"TRACK UNAVAILABLE");
        lv_label_set_text(track_label,track_readout);
        lv_label_set_text(library_track,track_readout);
        bool library_changed=library_first!=s.track_first || library_last!=s.track_last || library_current!=s.track;
        library_first=s.track_first; library_last=s.track_last; library_current=s.track;
        if(active_page==library) {
            if(library_changed || library_dirty) update_library();
            else for(int i=0;i<7;i++) lv_obj_set_state(library_rows[i],LV_STATE_DISABLED,!armed || busy);
        }
        lv_obj_set_style_bg_color(transport_light,lv_color_hex(s.paused?0xe6b75e:s.playing?0x8bd89f:0x303d36),0);
        for(int i=0;i<4;i++) { uint32_t c=s.pack_on && i==(reduced_motion?0:(int)((lv_tick_get()/250)%4))?0xff5949:0x452622; lv_obj_set_style_bg_color(dots[i],lv_color_hex(c),0); lv_obj_set_style_bg_color(lv_obj_get_child(monitor_dots,i),lv_color_hex(c),0); }
        int v[] = {s.master,s.effects,s.music};
        for (int i=0;i<3;i++) {
            if ((!dragging[i] && !volume_pending[i]) || !armed) {
                dragging[i]=false;
                lv_slider_set_value(levels[i],v[i],LV_ANIM_OFF);
                lv_slider_set_value(levels[i+3],v[i],LV_ANIM_OFF);
                lv_label_set_text_fmt(level_labels[i+3],"%d",v[i]);
                lv_label_set_text_fmt(level_labels[i],"%d",v[i]); update_leds(i,v[i]);
            }
        }
        lv_label_set_text(play_label,s.playing ? "STOP" : "START");
        lv_obj_set_style_text_color(buttons[PL_PLAY_STOP],lv_color_hex(s.playing?0xe86b57:0x8bd89f),0);
        lv_label_set_text(pause_label,s.paused ? "RESUME" : "PAUSE");
    } else {
        lv_label_set_text(telemetry_label,"LINK LOST");
        lv_label_set_text(track_label,"---  /  NO LINK");
        lv_label_set_text(library_track,"TRACK UNAVAILABLE");
        library_first=library_last=library_current=-1; update_library();
        lv_obj_set_style_bg_color(transport_light,lv_color_hex(0x303d36),0);
        for(int i=0;i<4;i++) { lv_obj_set_style_bg_color(dots[i],lv_color_hex(0x452622),0); lv_obj_set_style_bg_color(lv_obj_get_child(monitor_dots,i),lv_color_hex(0x452622),0); }
        for (int i=0;i<3;i++) {
            dragging[i]=false;
            lv_slider_set_value(levels[i],0,LV_ANIM_OFF);
            lv_slider_set_value(levels[i+3],0,LV_ANIM_OFF);
            lv_label_set_text(level_labels[i+3],"--");
            lv_label_set_text(level_labels[i],"--"); update_leds(i,0);
        }
        lv_label_set_text(play_label,"START");
        lv_label_set_text(pause_label,"PAUSE");
    }
    update_subsystems(&s,armed,busy);
    for(int i=0;i<6;i++) {
        lv_obj_set_state(levels[i],LV_STATE_DISABLED,!armed || busy);
        if(i<3) lv_obj_set_state(quick_modules[i],LV_STATE_DISABLED,!armed || busy);
        else lv_obj_set_state(level_labels[i],LV_STATE_DISABLED,!armed || busy);
    }
    for(int i=0;i<=PL_NEXT;i++) {
        bool disabled=!armed || busy || !pl_command_path((pl_action_t)i,&s);
        if(buttons[i]) lv_obj_set_state(buttons[i],LV_STATE_DISABLED,disabled);
        if(aux_buttons[i]) lv_obj_set_state(aux_buttons[i],LV_STATE_DISABLED,disabled);
    }
}
static lv_color_t desaturate(const lv_color_filter_dsc_t *filter, lv_color_t color, lv_opa_t strength)
{
    (void)filter;
    uint8_t gray=lv_color_luminance(color);
    return lv_color_mix(lv_color_make(gray,gray,gray),color,strength);
}
static void add_disabled_style(lv_obj_t *obj, lv_part_t part)
{
    lv_obj_add_style(obj,&disabled_style,part | LV_STATE_DISABLED);
}
static lv_obj_t *make_button(lv_obj_t *parent,const char *text,int width,int height,lv_obj_t **label_out)
{
    lv_obj_t *b=lv_button_create(parent);
    lv_obj_remove_flag(b,LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_size(b,width,height);
    lv_obj_set_style_bg_color(b,lv_color_hex(0x202829),0);
    lv_obj_set_style_text_color(b,lv_color_hex(0xd2d6cd),0);
    lv_obj_set_style_border_color(b,lv_color_hex(0x626d6c),0);
    lv_obj_set_style_border_width(b,1,0);
    lv_obj_set_style_radius(b,4,0);
    lv_obj_set_style_shadow_width(b,0,0);
    add_disabled_style(b,LV_PART_MAIN);
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
static int track_at(int offset)
{
    return library_playlist<0?library_first+offset:pl_catalog_playlist_track(library_playlist,offset);
}

static bool show_track(int offset)
{
    int id=track_at(offset);
    return id>=library_first && id<=library_last && (!favorites_only || pl_catalog_favorite(id)) &&
           (!library_search[0] || pl_catalog_match(id,library_first,library_search));
}

static void update_library(void)
{
    library_dirty=false;
    bool valid=pl_catalog_range(library_first,library_last);
    int count=library_playlist<0?library_last-library_first+1:PL_PLAYLIST_TRACKS;
    if(!valid || library_offset<0 || library_offset>=count) library_offset=0;
    int at=library_offset;
    library_has_previous=false;
    for(int prev=at-1;valid && prev>=0;prev--) {
        if(show_track(prev)) { library_has_previous=true; break; }
    }
    for(int i=0;i<7;i++) {
        while(valid && at<count && !show_track(at)) at++;
        bool present=valid && at<count;
        int id=track_at(at);
        visible_tracks[i]=present?id:-1;
        if(!present) {
            lv_obj_add_flag(library_rows[i],LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(library_rows[i],LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_state(library_rows[i],LV_STATE_USER_1,id==library_current);
        char fallback[24];
        lv_label_set_text_fmt(library_names[i],"%s %02d    %s    %d%s",id==library_current?">":" ",
                              id-library_first+1,pl_catalog_title(id,library_first,fallback),id,pl_catalog_favorite(id)?"  *":"");
        lv_obj_set_state(library_rows[i],LV_STATE_DISABLED,!pl_is_unlocked() || pl_busy());
        at++;
    }
    while(valid && at<count && !show_track(at)) at++;
    next_track_offset=valid && at<count?at:-1;
    if(valid && visible_tracks[0]<0) lv_obj_remove_flag(library_empty,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(library_empty,LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_state(library_prev,LV_STATE_DISABLED,!library_has_previous);
    lv_obj_set_state(library_next,LV_STATE_DISABLED,next_track_offset<0);
    bool current_valid=valid && library_current>=library_first && library_current<=library_last;
    lv_obj_set_state(library_favorite,LV_STATE_DISABLED,!current_valid);
    lv_label_set_text(lv_obj_get_child(library_favorite,0),
                      current_valid && pl_catalog_favorite(library_current)?"UNSTAR CURRENT":"STAR CURRENT");
    lv_label_set_text(lv_obj_get_child(library_filter,0),library_playlist>=0?"PLAYLIST":favorites_only?"FAVORITES":"ALL TRACKS");
    lv_label_set_text(lv_obj_get_child(library_return,0),
                      library_origin==music_page?"RETURN TO MUSIC":"RETURN TO FIELD CONTROL");
}

enum { PREVIOUS_PAGE=-1, NEXT_PAGE=-2, FILTER_TRACKS=-3, STAR_CURRENT=-4, RETURN_FROM_LIBRARY=-5 };
static void library_event(lv_event_t *e)
{
    if(field_locked) return;
    page_entered=lv_tick_get();
    int i=(int)(intptr_t)lv_event_get_user_data(e);
    if(i==RETURN_FROM_LIBRARY) {
        show_page(library_origin);
        return;
    }
    if(i==FILTER_TRACKS) {
        favorites_only=library_playlist<0 && !favorites_only;
        library_playlist=-1;
        library_offset=0;
    } else if(i==STAR_CURRENT) {
        if(library_current<library_first || library_current<500 || library_current>library_last || library_current>4595) return;
        pl_catalog_star(library_current);
        if(favorites_only) library_offset=0;
    } else if(i==PREVIOUS_PAGE) {
        int count=0;
        for(int offset=library_offset-1;offset>=0 && count<7;offset--) {
            if(show_track(offset)) { library_offset=offset; count++; }
        }
    } else if(i==NEXT_PAGE) {
        if(next_track_offset>=0) library_offset=next_track_offset;
    } else if(ui_online && visible_tracks[i]>=500 && pl_request_track(visible_tracks[i])) {
        notice="TRACK REQUEST QUEUED"; notice_at=lv_tick_get(); show_page(library_origin);
    }
    update_library();
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
    lv_obj_remove_flag(b,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_GESTURE_BUBBLE); lv_obj_add_flag(b,LV_OBJ_FLAG_CLICKABLE);
    for(int i=0;i<4;i++) {
        lv_obj_t *d=lv_obj_create(b); lv_obj_remove_style_all(d); lv_obj_set_size(d,15,15);
        lv_obj_set_pos(d,8+i*24,8); lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0);
        lv_obj_set_style_bg_opa(d,LV_OPA_COVER,0); lv_obj_set_style_bg_color(d,lv_color_hex(0x452622),0);
        lv_obj_set_style_border_width(d,2,0); lv_obj_set_style_border_color(d,lv_color_hex(0x626d6c),0);
        lv_obj_remove_flag(d,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    }
    return b;
}
static void menu_surface(lv_obj_t *obj, uint32_t color, int radius, bool border)
{
    lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    lv_obj_set_style_radius(obj,radius,0);
    lv_obj_set_style_border_width(obj,border?1:0,0);
    lv_obj_set_style_border_color(obj,lv_color_hex(MENU_EDGE),0);
}

static lv_obj_t *menu_panel(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius,bool border)
{
    lv_obj_t *panel=lv_obj_create(parent);
    lv_obj_remove_style_all(panel);
    lv_obj_set_pos(panel,x,y);
    lv_obj_set_size(panel,w,h);
    lv_obj_remove_flag(panel,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
    menu_surface(panel,color,radius,border);
    return panel;
}

static lv_obj_t *menu_button(lv_obj_t *parent,const char *text,int width,int height,lv_obj_t **out)
{
    lv_obj_t *button=make_button(parent,text,width,height,out);
    menu_surface(button,0x262c2e,8,true);
    lv_obj_set_style_bg_grad_color(button,lv_color_hex(0x101416),0);
    lv_obj_set_style_bg_grad_dir(button,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_bg_grad_color(button,lv_color_hex(0x17292f),LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button,lv_color_hex(MENU_TEXT),0);
    lv_obj_set_style_text_font(button,&lv_font_montserrat_14,0);
    lv_obj_set_style_bg_color(button,lv_color_hex(0x2a444d),LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button,lv_color_hex(0x73949a),LV_STATE_PRESSED);
    return button;
}

static void header_button(lv_obj_t *button)
{
    menu_surface(button,MENU_BAR,6,true);
    lv_obj_set_style_text_color(button,lv_color_hex(MENU_MUTED),0);
    lv_obj_set_style_text_font(button,&lv_font_montserrat_12,0);
}

static lv_obj_t *menu_value(lv_obj_t *page,const char *name,int x,int y,int w,int h)
{
    lv_obj_t *card=menu_panel(page,x,y,w,h,MENU_CARD,8,true);
    lv_obj_t *key=label_at(card,name,11,7,MENU_MUTED);
    lv_obj_set_style_text_font(key,&lv_font_montserrat_12,0);
    lv_obj_t *value=label_at(card,"",11,24,MENU_TEXT);
    lv_obj_set_size(value,w-22,19);
    lv_label_set_long_mode(value,LV_LABEL_LONG_MODE_DOTS);
    return value;
}

static void menu_title(lv_obj_t *page,const char *title,int height)
{
    menu_panel(page,0,0,410,height,MENU_BAR,0,false);
    menu_panel(page,0,height-1,410,1,MENU_EDGE,0,false);
    lv_obj_t *heading=label_at(page,title,0,0,MENU_TEXT);
    lv_obj_set_style_text_letter_space(heading,1,0);
    lv_obj_align(heading,LV_ALIGN_TOP_MID,0,(height-16)/2);
}

static lv_obj_t *navigation(lv_obj_t *page, const char *text, int x, int y, int width, lv_obj_t *dest)
{
    lv_obj_t *button=menu_button(page,text,width,45,NULL);
    lv_obj_set_pos(button,x,y);
    lv_obj_add_event_cb(button,nav_event,LV_EVENT_CLICKED,dest);
    return button;
}

static void lock_event(lv_event_t *e)
{
    (void)e;
    engage_lock();
}

static lv_obj_t *subsystem_page(lv_obj_t *root, const char *title, lv_obj_t *back)
{
    lv_obj_t *page=page_new(root,MENU_BG);
    menu_title(page,title,60);
    lv_obj_t *back_button=navigation(page,"BACK",16,8,68,back);
    header_button(back_button);
    lv_obj_t *lock=menu_button(page,"LOCK",68,45,NULL);
    lv_obj_set_pos(lock,326,8);
    header_button(lock);
    lv_obj_add_event_cb(lock,lock_event,LV_EVENT_CLICKED,NULL);
    return page;
}

static const char *ready_text(bool known, bool ready)
{
    return !known?"UNKNOWN":ready?"READY":"CHECK";
}

static void update_subsystems(const protonlink_status_t *s, bool armed, bool busy)
{
    lv_label_set_text(menu_summary,ui_online?(s->pack_on?"PACK ONLINE":"PACK STANDBY"):"TELEMETRY UNAVAILABLE");
    lv_obj_set_style_text_color(menu_indicator,lv_color_hex(ui_online?MENU_CYAN:MENU_MUTED),0);
    lv_obj_set_style_text_color(menu_summary,lv_color_hex(ui_online?MENU_AMBER:MENU_MUTED),0);
    const char *lid=!s->lid_known?"Unknown":s->lid_fitted?"Attached":"Removed";
    const char *pack[]={s->pack_on?"Powered":"Idle",s->theme,s->stream,s->mode};
    const char *diag[]={pack[0],s->wand,s->cable,lid,s->wand_power,s->barrel,
                        s->cyclotron,s->temperature,s->firing?"Firing":"Idle"};
    for(int i=0;i<4;i++) lv_label_set_text(pack_values[i],ui_online?pack[i]:"—");
    bool attention[]={false,!strcmp(s->wand,"Not Connected"),!strcmp(s->cable,"Disconnected"),
                      s->lid_known && !s->lid_fitted,false,false,
                      !strcmp(s->cyclotron,"Critical") || !strcmp(s->cyclotron,"Warning"),
                      !strcmp(s->temperature,"Venting"),s->firing};
    for(int i=0;i<9;i++) {
        lv_label_set_text(diagnostic_values[i],ui_online?diag[i]:"—");
        lv_obj_set_style_text_color(diagnostic_values[i],lv_color_hex(!ui_online?MENU_MUTED:
                                    attention[i]?MENU_ALERT:MENU_ACTIVE),0);
    }
    lv_label_set_text(music_summary,ui_online?track_readout:"TELEMETRY UNAVAILABLE");
    lv_label_set_text(music_state,!ui_online || !s->music_known?"TRANSPORT UNAVAILABLE":
                      s->paused?"PAUSED":s->playing?"PLAYING":"STOPPED");
    bool known_cyclotron=!strcmp(s->cyclotron,"Normal") || !strcmp(s->cyclotron,"Active");
    bool ready=!s->firing && !s->ramping && !s->lockout && known_cyclotron && !strcmp(s->temperature,"Normal");
    const char *checks[]={ready_text(true,ui_online),ready_text(true,ui_online),
                         ready_text(ui_online && strcmp(s->cable,"Unknown")!=0,!strcmp(s->cable,"Connected")),
                         ready_text(ui_online && s->lid_known,s->lid_fitted),
                         ready_text(ui_online && (s->transition_known || s->legacy_superhero),ready)};
    for(int i=0;i<5;i++) {
        lv_label_set_text(readiness_values[i],checks[i]);
        lv_obj_set_style_text_color(readiness_values[i],lv_color_hex(!strcmp(checks[i],"READY")?MENU_ACTIVE:
                                    !strcmp(checks[i],"CHECK")?MENU_AMBER:MENU_MUTED),0);
    }
    lv_label_set_text(music_play,ui_online && s->playing?"STOP":"START");
    lv_label_set_text(music_pause,ui_online && s->paused?"RESUME":"PAUSE");
    for(int i=0;i<PL_OPT_COUNT;i++) {
        bool reported=(s->advertised_options & (1u<<i))!=0;
        lv_obj_set_state(option_buttons[i],LV_STATE_DISABLED,!armed || busy || !pl_option_path(i,s));
        lv_obj_t *label=lv_obj_get_child(option_buttons[i],0);
        const char *text=pl_option_label(i,s);
        if(strcmp(lv_label_get_text(label),text)) lv_label_set_text(label,text);
        if(reported) lv_obj_remove_flag(option_buttons[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(option_buttons[i],LV_OBJ_FLAG_HIDDEN);
    }
    bool pending=volume_pending[0] || volume_pending[1] || volume_pending[2];
    for(int i=0;i<5;i++) lv_obj_set_state(preset_buttons[i],LV_STATE_DISABLED,!armed || busy || pending);
}

static const int presets[][3]={{75,80,70},{45,50,40},{85,90,75},{20,25,15}};
static void preset_event(lv_event_t *e)
{
    protonlink_status_t s;
    if(field_locked || !pl_is_unlocked() || pl_busy() || !protonlink_snapshot(&s)) return;
    page_entered=lv_tick_get();
    int preset=(int)(intptr_t)lv_event_get_user_data(e);
    for(int i=0;i<3;i++) {
        if(volume_pending[i] || dragging[i]) return;
    }
    for(int i=0;i<3;i++) {
        int current=pl_volume_value(&s,i);
        if(preset<4) volume_target[i]=presets[preset][i];
        else if(audio_reduced) volume_target[i]=previous_audio[i];
        else {
            previous_audio[i]=current;
            volume_target[i]=((current+5)/10)*5;
        }
        volume_pending[i]=current!=volume_target[i];
        volume_due[i]=lv_tick_get();
    }
    audio_reduced=preset==4 && !audio_reduced;
    lv_label_set_text(reduction_label,audio_reduced?"RESTORE PREVIOUS AUDIO":"TEMPORARY 50% AUDIO REDUCTION");
}

static void preference_event(lv_event_t *e)
{
    if(field_locked) return;
    page_entered=lv_tick_get();
    lv_obj_t *label=lv_obj_get_child(lv_event_get_target(e),0);
    if(lv_event_get_user_data(e)) {
        reduced_motion=!reduced_motion;
        lv_label_set_text(label,reduced_motion?"REDUCED MOTION: ON":"REDUCED MOTION: OFF");
    } else {
        command_home=!command_home;
        lv_label_set_text(label,command_home?"MAIN SCREEN: FIELD LOCK":"MAIN SCREEN: INDUSTRIAL CONTROL");
    }
    save_preferences();
}

static void save_preferences(void)
{
#ifndef PROTONLINK_DESKTOP
    uint32_t flags=(uint32_t)selected_face | ((uint32_t)marquee_palette<<4) | ((uint32_t)dynamic_faces<<6) |
        ((uint32_t)!contact_labels<<7) | ((uint32_t)reduced_motion<<8) | ((uint32_t)command_home<<9) |
        ((uint32_t)!horizontal_gestures<<10);
    for(int i=0;i<4;i++) flags|=((uint32_t)module_order[i]<<(12+i*2)) | ((uint32_t)module_hidden[i]<<(20+i));
    nvs_handle_t nvs;
    if(nvs_open("pl_display",NVS_READWRITE,&nvs)==ESP_OK) {
        if(nvs_set_u32(nvs,"settings",flags)==ESP_OK) nvs_commit(nvs);
        nvs_close(nvs);
    }
#endif
}

static void load_preferences(void)
{
#ifndef PROTONLINK_DESKTOP
    nvs_handle_t nvs;
    if(nvs_open("pl_display",NVS_READONLY,&nvs)==ESP_OK) {
        uint32_t flags;
        if(nvs_get_u32(nvs,"settings",&flags)==ESP_OK) {
            if((flags&15)<PL_FACE_COUNT) selected_face=flags&15;
            marquee_palette=(flags>>4)&3;
            dynamic_faces=(flags>>6)&1;
            contact_labels=!((flags>>7)&1);
            reduced_motion=(flags>>8)&1;
            command_home=(flags>>9)&1;
            horizontal_gestures=!((flags>>10)&1);
            uint8_t order[4];
            unsigned seen=0;
            for(int i=0;i<4;i++) {
                order[i]=(flags>>(12+i*2))&3;
                seen|=1u<<order[i];
                module_hidden[i]=(flags>>(20+i))&1;
            }
            if(seen==15) memcpy(module_order,order,sizeof(order));
        }
        nvs_close(nvs);
    }
#endif
    pl_face_options(contact_labels,marquee_colors[marquee_palette]);
}

static void subsystem_swipe(lv_event_t *e)
{
    (void)e;
    if(field_locked || !horizontal_gestures || gesture_active) return;
    lv_obj_t *pages[]={quick,monitor,music_page,diagnostics};
    lv_indev_t *indev=lv_indev_active();
    if(!indev) return;
    lv_dir_t direction=lv_indev_get_gesture_dir(indev);
    if(direction!=LV_DIR_LEFT && direction!=LV_DIR_RIGHT) return;
    for(int i=0;i<4;i++) {
        if(active_page==pages[i]) {
            lv_indev_wait_release(indev);
            show_page(pages[(i+(direction==LV_DIR_LEFT?1:3))%4]);
            return;
        }
    }
}

static void pick_face(lv_event_t *e)
{
    if(field_locked) return;
    selected_face=(selected_face+1)%PL_FACE_COUNT;
    save_preferences();
    lv_label_set_text_fmt(lv_obj_get_child(lv_event_get_target(e),0),"FACE: %s",pl_face_name(selected_face));
    page_entered=lv_tick_get();
}

static void display_option(lv_event_t *e)
{
    if(field_locked) return;
    lv_obj_t *label=lv_obj_get_child(lv_event_get_target(e),0);
    switch((intptr_t)lv_event_get_user_data(e)) {
    case 0:
        dynamic_faces=!dynamic_faces;
        lv_label_set_text(label,dynamic_faces?"DYNAMIC FACES: ON":"DYNAMIC FACES: OFF");
        break;
    case 1:
        contact_labels=!contact_labels;
        lv_label_set_text(label,contact_labels?"PKE CONTACT LABELS: ON":"PKE CONTACT LABELS: OFF");
        break;
    case 2:
        marquee_palette=(marquee_palette+1)%4;
        lv_label_set_text_fmt(label,"MARQUEE COLOR: %s",marquee_names[marquee_palette]);
        break;
    default:
        horizontal_gestures=!horizontal_gestures;
        lv_label_set_text(label,horizontal_gestures?"SWIPE NAVIGATION: ON":"SWIPE NAVIGATION: OFF");
        break;
    }
    save_preferences();
    pl_face_options(contact_labels,marquee_colors[marquee_palette]);
    page_entered=lv_tick_get();
}

static void update_layout(void)
{
    for(int row=0;row<4;row++) {
        int module=module_order[row];
        lv_obj_move_to_index(quick_modules[module],row+2);
        if(module_hidden[module]) lv_obj_add_flag(quick_modules[module],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(quick_modules[module],LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(module_labels[row],module_names[module]);
        lv_label_set_text(lv_obj_get_child(module_toggle[row],0),module_hidden[module]?"SHOW":"HIDE");
        lv_obj_set_state(module_move[row],LV_STATE_DISABLED,row==0);
    }
}

static void layout_event(lv_event_t *e)
{
    if(field_locked) return;
    page_entered=lv_tick_get();
    int option=(int)(intptr_t)lv_event_get_user_data(e);
    int row=option/2;
    int module=module_order[row];
    if(option%2) module_hidden[module]=!module_hidden[module];
    else if(row>0) {
        module_order[row]=module_order[row-1];
        module_order[row-1]=module;
    }
    update_layout();
    save_preferences();
}

static void subsystem_action(lv_obj_t *row, pl_action_t action, const char *text, int width, lv_obj_t **label)
{
    bool hold=action==PL_VENT;
    lv_obj_t *button=menu_button(row,text,width,45,label);
    aux_buttons[action]=button;
    lv_obj_set_style_text_color(button,lv_color_hex(hold?switch_color[PL_VENT]:0x8bd89f),0);
    lv_obj_add_event_cb(button,hold?gesture_event:command,hold?LV_EVENT_ALL:LV_EVENT_CLICKED,(void *)(intptr_t)action);
    if(hold) manual_vent_progress=progress_at(button,width-24,-2,switch_color[PL_VENT]);
}

static void option_event(lv_event_t *e)
{
    if(field_locked) return;
    page_entered=lv_tick_get();
    pl_option_t option=(pl_option_t)(intptr_t)lv_event_get_user_data(e);
    notice=pl_request_option(option)?"REQUEST QUEUED":"REQUEST BLOCKED / BUSY";
    notice_at=lv_tick_get();
}

static void option_button(lv_obj_t *parent, pl_option_t option, int x, int y, int width)
{
    protonlink_status_t empty={0};
    lv_obj_t *button=option_buttons[option]=menu_button(parent,pl_option_label(option,&empty),width,42,NULL);
    lv_obj_set_pos(button,x,y);
    lv_obj_add_event_cb(button,option_event,LV_EVENT_CLICKED,(void *)(intptr_t)option);
}

static void create_subsystems(lv_obj_t *root)
{
    pack_info=subsystem_page(root,"PACK INFO",menu);
    music_page=subsystem_page(root,"MUSIC",menu);
    mixer=subsystem_page(root,"AUDIO MIXER",menu);
    diagnostics=subsystem_page(root,"DIAGNOSTICS",menu);
    readiness=subsystem_page(root,"STARTUP CHECK",pack_info);
    settings=subsystem_page(root,"SETTINGS",menu);
    audio_presets=subsystem_page(root,"AUDIO PRESETS",settings);
    field_layout=subsystem_page(root,"FIELD LAYOUT",settings);
    display_page=subsystem_page(root,"DISPLAY",settings);
    effects_page=subsystem_page(root,"EFFECTS",menu);
    era_page=subsystem_page(root,"MOVIE ERA",effects_page);
    stream_page=subsystem_page(root,"STREAM MODE",effects_page);

    menu_title(menu,"COMMAND",60);
    lv_obj_t *lock=menu_button(menu,"LOCK",68,45,NULL);
    lv_obj_set_pos(lock,326,8);
    header_button(lock);
    lv_obj_add_event_cb(lock,lock_event,LV_EVENT_CLICKED,NULL);
    lv_obj_t *status=menu_panel(menu,24,76,362,82,0x10242b,12,true);
    lv_obj_set_style_bg_grad_color(status,lv_color_hex(0x060c10),0);
    lv_obj_set_style_bg_grad_dir(status,LV_GRAD_DIR_VER,0);
    menu_indicator=label_at(status,LV_SYMBOL_POWER,0,0,MENU_CYAN);
    lv_obj_set_style_text_font(menu_indicator,&lv_font_montserrat_24,0);
    lv_obj_align(menu_indicator,LV_ALIGN_RIGHT_MID,-18,0);
    menu_summary=label_at(status,"",14,0,MENU_AMBER);
    lv_obj_set_style_text_font(menu_summary,&lv_font_montserrat_24,0);
    lv_obj_set_size(menu_summary,282,30);
    lv_label_set_long_mode(menu_summary,LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(menu_summary,LV_ALIGN_LEFT_MID,14,0);

    static const char *captions[]={"PACK","MUSIC","MIXER","EFFECTS","STATUS","SETUP"};
    static const char *icons[]={LV_SYMBOL_POWER,LV_SYMBOL_AUDIO,LV_SYMBOL_VOLUME_MID,
                                LV_SYMBOL_CHARGE,LV_SYMBOL_BARS,LV_SYMBOL_SETTINGS};
    lv_obj_t *destinations[]={pack_info,music_page,mixer,effects_page,diagnostics,settings};
    for(int i=0;i<6;i++) {
        lv_obj_t *tile=navigation(menu,captions[i],24+i%3*123,174+i/3*102,116,destinations[i]);
        lv_obj_set_height(tile,92);
        lv_obj_align(lv_obj_get_child(tile,0),LV_ALIGN_BOTTOM_MID,0,-9);
        lv_obj_t *icon=label_at(tile,icons[i],0,0,MENU_CYAN);
        lv_obj_set_style_text_font(icon,&lv_font_montserrat_24,0);
        lv_obj_align(icon,LV_ALIGN_TOP_MID,0,10);
    }
    menu_panel(menu,24,424,362,1,MENU_EDGE,0,false);
    navigation(menu,"PKE MONITOR",24,439,178,monitor);
    navigation(menu,"CONTROL",208,439,178,quick);

    static const char *pack_keys[]={"PACK","MOVIE ERA","STREAM","SYSTEM MODE"};
    for(int i=0;i<4;i++) pack_values[i]=menu_value(pack_info,pack_keys[i],24+i%2*184,78+i/2*65,178,55);
    lv_obj_t *vent_row=line(pack_info,50);
    lv_obj_set_pos(vent_row,12,235);
    subsystem_action(vent_row,PL_VENT,"MANUAL VENT",362,NULL);
    navigation(pack_info,"STARTUP CHECK",24,296,362,readiness);
    navigation(pack_info,"DIAGNOSTICS",24,352,362,diagnostics);
    navigation(pack_info,"SETTINGS",24,408,362,settings);
    static const char *check_keys[]={"EQUIPMENT LINK","TELEMETRY CURRENT","RIBBON CONNECTED",
                                     "CYCLOTRON LID FITTED","PACK IDLE OR NORMAL"};
    for(int i=0;i<5;i++) {
        lv_obj_t *card=menu_panel(readiness,24,78+i*67,362,55,MENU_CARD,8,true);
        label_at(card,check_keys[i],13,17,MENU_TEXT);
        readiness_values[i]=label_at(card,"",0,0,MENU_MUTED);
        lv_obj_align(readiness_values[i],LV_ALIGN_RIGHT_MID,-12,0);
        lv_obj_set_style_text_font(readiness_values[i],&lv_font_montserrat_12,0);
    }
    diagnostics_art=pl_face_create(diagnostics,72,120);
    static const char *diag_keys[]={"PACK","WAND","RIBBON","LID","WAND POWER","BARREL","CYCLOTRON","VENT STATE","FIRING"};
    for(int i=0;i<9;i++) diagnostic_values[i]=menu_value(diagnostics,diag_keys[i],24+i%2*184,200+i/2*57,178,51);

    lv_obj_t *now_playing=menu_panel(music_page,24,73,362,70,MENU_CARD,8,true);
    music_state=label_at(now_playing,"",12,10,MENU_MUTED);
    lv_obj_set_style_text_font(music_state,&lv_font_montserrat_12,0);
    music_summary=label_at(now_playing,"",12,31,MENU_ACTIVE);
    lv_obj_set_size(music_summary,335,28);
    lv_label_set_long_mode(music_summary,LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_font(music_summary,&lv_font_montserrat_24,0);
    music_art=pl_face_create(music_page,156,85);
    lv_obj_t *keys=line(music_page,58);
    lv_obj_set_pos(keys,12,250);
    subsystem_action(keys,PL_PREVIOUS,"|<",61,NULL);
    subsystem_action(keys,PL_PLAY_STOP,"START",110,&music_play);
    subsystem_action(keys,PL_PAUSE_RESUME,"PAUSE",110,&music_pause);
    subsystem_action(keys,PL_NEXT,">|",61,NULL);
    navigation(music_page,"TRACK LIBRARY",24,320,362,library);
    for(int i=PL_OPT_MUTE;i<=PL_OPT_SHUFFLE;i++) option_button(music_page,i,24+i*121,380,116);
    for(int i=PL_OPT_SMOKE;i<=PL_OPT_DIRECTION;i++) option_button(effects_page,i,24,80+(i-PL_OPT_SMOKE)*53,362);
    navigation(effects_page,"MOVIE ERA",24,269,362,era_page);
    navigation(effects_page,"STREAM MODE",24,326,362,stream_page);
    navigation(effects_page,"PACK INFO",24,383,362,pack_info);
    label_at(effects_page,"Controls depend on reported pack support.",24,452,0x83999f);
    label_at(era_page,"Requires idle compatible equipment.",24,72,0x83999f);
    label_at(stream_page,"Requires a reported stream-change window.",24,72,0x83999f);
    for(int i=PL_OPT_1984;i<=PL_OPT_2024;i++) option_button(era_page,i,24,109+(i-PL_OPT_1984)*53,362);
    for(int i=PL_OPT_PROTON;i<=PL_OPT_CUSTOM;i++) {
        int at=i-PL_OPT_PROTON;
        option_button(stream_page,i,24+(at%2)*184,109+(at/2)*56,178);
    }

    static const char *channels[]={"MASTER","EFFECTS","MUSIC"};
    for(int i=0;i<3;i++) {
        int x=78+i*127;
        lv_obj_t *fader=menu_panel(mixer,x-54,75,108,338,MENU_CARD,10,true);
        lv_obj_set_style_bg_grad_color(fader,lv_color_hex(0x18242a),0);
        lv_obj_set_style_bg_grad_dir(fader,LV_GRAD_DIR_HOR,0);
        lv_obj_t *name=label_at(mixer,channels[i],x-38,88,0xd2d6cd);
        lv_obj_set_width(name,76);
        lv_obj_set_style_text_align(name,LV_TEXT_ALIGN_CENTER,0);
        level_labels[i+3]=label_at(mixer,"--",x-25,124,channel_color[i]);
        lv_obj_set_width(level_labels[i+3],50);
        lv_obj_set_style_text_align(level_labels[i+3],LV_TEXT_ALIGN_CENTER,0);
        lv_obj_set_style_text_font(level_labels[i+3],&lv_font_montserrat_24,0);
        lv_obj_t *slider=levels[i+3]=lv_slider_create(mixer);
        lv_obj_remove_flag(slider,LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_set_size(slider,26,220);
        lv_obj_set_pos(slider,x-13,169);
        lv_slider_set_range(slider,0,100);
        lv_obj_set_style_bg_color(slider,lv_color_hex(MENU_BG),LV_PART_MAIN);
        lv_obj_set_style_bg_color(slider,lv_color_hex(channel_color[i]),LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider,lv_color_hex(0x8b9897),LV_PART_KNOB);
        add_disabled_style(slider,LV_PART_MAIN);
        add_disabled_style(slider,LV_PART_INDICATOR);
        add_disabled_style(slider,LV_PART_KNOB);
        add_disabled_style(level_labels[i+3],LV_PART_MAIN);
        lv_obj_add_event_cb(slider,slider_event,LV_EVENT_ALL,(void *)(intptr_t)(i+3));
    }
    navigation(mixer,"AUDIO PRESETS",24,433,362,audio_presets);

    navigation(settings,"AUDIO PRESETS",24,85,362,audio_presets);
    const char *preferences[]={command_home?"MAIN SCREEN: FIELD LOCK":"MAIN SCREEN: INDUSTRIAL CONTROL",
                               reduced_motion?"REDUCED MOTION: ON":"REDUCED MOTION: OFF"};
    for(int i=0;i<2;i++) {
        lv_obj_t *button=menu_button(settings,preferences[i],362,45,NULL);
        lv_obj_set_pos(button,24,143+58*i);
        lv_obj_add_event_cb(button,preference_event,LV_EVENT_CLICKED,(void *)(intptr_t)i);
        if(i==0) main_screen_label=lv_obj_get_child(button,0);
    }
    navigation(settings,"FIELD LAYOUT",24,259,362,field_layout);
    navigation(settings,"OPEN INDUSTRIAL CONTROL",24,317,362,quick);
    navigation(settings,"DISPLAY SETTINGS",24,375,362,display_page);
    char face_title[64], color_title[42];
    snprintf(face_title,sizeof(face_title),"FACE: %s",pl_face_name(selected_face));
    snprintf(color_title,sizeof(color_title),"MARQUEE COLOR: %s",marquee_names[marquee_palette]);
    lv_obj_t *face=menu_button(display_page,face_title,362,45,NULL);
    lv_obj_set_pos(face,24,85);
    lv_obj_add_event_cb(face,pick_face,LV_EVENT_CLICKED,NULL);
    const char *display_options[]={dynamic_faces?"DYNAMIC FACES: ON":"DYNAMIC FACES: OFF",
                                   contact_labels?"PKE CONTACT LABELS: ON":"PKE CONTACT LABELS: OFF",
                                   color_title,horizontal_gestures?"SWIPE NAVIGATION: ON":"SWIPE NAVIGATION: OFF"};
    for(int i=0;i<4;i++) {
        lv_obj_t *button=menu_button(display_page,display_options[i],362,45,NULL);
        lv_obj_set_pos(button,24,144+59*i);
        lv_obj_add_event_cb(button,display_option,LV_EVENT_CLICKED,(void *)(intptr_t)i);
    }
    for(int row=0;row<4;row++) {
        int y=85+59*row;
        module_labels[row]=label_at(field_layout,"",24,y+13,0xd2d6cd);
        module_move[row]=menu_button(field_layout,"MOVE UP",102,43,NULL);
        module_toggle[row]=menu_button(field_layout,"HIDE",94,43,NULL);
        lv_obj_set_pos(module_move[row],176,y);
        lv_obj_set_pos(module_toggle[row],292,y);
        lv_obj_add_event_cb(module_move[row],layout_event,LV_EVENT_CLICKED,(void *)(intptr_t)(2*row));
        lv_obj_add_event_cb(module_toggle[row],layout_event,LV_EVENT_CLICKED,(void *)(intptr_t)(2*row+1));
    }
    update_layout();

    const char *labels[]={"CONVENTION   75 / 80 / 70","INDOOR   45 / 50 / 40","PARADE   85 / 90 / 75",
                          "QUIET   20 / 25 / 15","TEMPORARY 50% AUDIO REDUCTION"};
    for(int i=0;i<5;i++) {
        preset_buttons[i]=menu_button(audio_presets,labels[i],362,46,i==4?&reduction_label:NULL);
        lv_obj_set_pos(preset_buttons[i],24,85+59*i);
        lv_obj_add_event_cb(preset_buttons[i],preset_event,LV_EVENT_CLICKED,(void *)(intptr_t)i);
    }
}

static void catalog_note(const char *text)
{
    lv_label_set_text(catalog_notice,text);
}

static void refresh_profiles(void)
{
    lv_label_set_text_fmt(profile_label,"PROFILE %d / %d\n%s",pl_catalog_profile()+1,PL_PROFILE_COUNT,pl_catalog_profile_name());
    for(int i=0;i<PL_PLAYLIST_LIMIT;i++) {
        const char *name=pl_catalog_playlist_name(i);
        lv_label_set_text_fmt(lv_obj_get_child(playlist_buttons[i],0),"%d   %s",i+1,*name?name:"Empty");
    }
}

static void update_song_sync(const protonlink_status_t *s)
{
    if(!sync_waiting || protonlink_song_list(NULL,0,NULL,NULL)==PL_SYNC_PENDING) return;
    static PL_EXTERNAL_BSS char list[PL_SONG_LIST_SIZE];
    int first=0,last=0;
    pl_sync_state_t result=protonlink_song_list(list,sizeof(list),&first,&last);
    sync_waiting=false;
    if(result==PL_SYNC_READY && s->online && first==s->track_first && last==s->track_last &&
       pl_catalog_import(list,first,last,true)) {
        library_dirty=true;
        catalog_note("Device track titles synced");
    }
    else catalog_note("Sync unavailable. Existing titles kept.");
}

static void sync_titles(lv_event_t *e)
{
    (void)e;
    if(field_locked || sync_waiting) return;
    page_entered=lv_tick_get();
    sync_waiting=protonlink_request_song_list();
    catalog_note(sync_waiting?"Reading device track names...":"No track range or equipment link");
}

static void editor_focus(lv_event_t *e)
{
    lv_keyboard_set_textarea(edit_keyboard,lv_event_get_target(e));
}

static void editor_open(int kind, int slot)
{
    show_page(catalog_editor);
    edit_kind=kind;
    edit_slot=slot;
    edit_track=library_current;
    lv_textarea_set_one_line(edit_value,kind!=EDIT_IMPORT);
    lv_obj_set_height(edit_value,kind==EDIT_IMPORT?101:45);
    lv_textarea_set_max_length(edit_value,kind==EDIT_IMPORT?PL_SONG_LIST_SIZE-1:PL_TITLE_SIZE-1);
    if(kind==EDIT_PLAYLIST) lv_obj_remove_flag(edit_ids,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(edit_ids,LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(edit_ids,"");
    lv_textarea_set_text(edit_value,"");
    lv_keyboard_set_textarea(edit_keyboard,edit_value);
    switch(kind) {
    case EDIT_SEARCH:
        lv_label_set_text(edit_prompt,"SEARCH BY TITLE OR TRACK ID");
        lv_textarea_set_text(edit_value,library_search);
        break;
    case EDIT_TITLE:
        lv_label_set_text_fmt(edit_prompt,"LOCAL TITLE / TRACK %d",edit_track);
        break;
    case EDIT_IMPORT:
        lv_label_set_text(edit_prompt,"TEXT, CSV, JSON OR M3U TITLES");
        break;
    case EDIT_PLAYLIST: {
        lv_label_set_text(edit_prompt,"NAME AND TRACK IDS IN ORDER");
        lv_textarea_set_text(edit_value,pl_catalog_playlist_name(slot));
        char ids[PL_PLAYLIST_TRACKS*6+1]="";
        size_t used=0;
        for(int i=0;i<PL_PLAYLIST_TRACKS;i++) {
            int id=pl_catalog_playlist_track(slot,i);
            if(!id) break;
            used+=(size_t)snprintf(ids+used,sizeof(ids)-used,"%s%d",i?",":"",id);
        }
        lv_textarea_set_text(edit_ids,ids);
        break;
    }
    case EDIT_PROFILE:
        lv_label_set_text(edit_prompt,"PACK PROFILE NAME");
        lv_textarea_set_text(edit_value,pl_catalog_profile_name());
        break;
    }
}

#ifdef PROTONLINK_DESKTOP
bool protonlink_ui_paste_text(const char *text)
{
    if(field_locked || active_page!=catalog_editor || !text || strlen(text)>=PL_SONG_LIST_SIZE) return false;
    lv_obj_t *target=lv_keyboard_get_textarea(edit_keyboard);
    if(!target) return false;
    lv_textarea_add_text(target,text);
    page_entered=lv_tick_get();
    return true;
}
#endif

static void editor_action(lv_event_t *e)
{
    if(field_locked) return;
    bool save=lv_event_get_user_data(e)!=NULL;
    const char *value=lv_textarea_get_text(edit_value);
    lv_obj_t *destination=catalog_tools;
    bool ok=true;
    switch(edit_kind) {
    case EDIT_SEARCH:
        destination=library;
        if(save) {
            snprintf(library_search,sizeof(library_search),"%s",value);
            library_offset=0;
            update_library();
        }
        break;
    case EDIT_TITLE:
        if(save) ok=edit_track>=library_first && edit_track<=library_last && pl_catalog_edit(edit_track,value);
        break;
    case EDIT_IMPORT:
        if(save) ok=*value && pl_catalog_import(value,library_first,library_last,false);
        break;
    case EDIT_PLAYLIST:
        destination=playlist_page;
        if(save) {
            ok=pl_catalog_save_playlist(edit_slot,value,lv_textarea_get_text(edit_ids),library_first,library_last);
            if(ok && library_playlist==edit_slot) library_offset=0;
            refresh_profiles();
        }
        break;
    case EDIT_PROFILE:
        destination=profile_page;
        if(save) { pl_catalog_name_profile(value); refresh_profiles(); }
        break;
    }
    if(!ok) {
        lv_label_set_text(edit_prompt,"INVALID INPUT OR CATALOG FULL");
        return;
    }
    if(save && edit_kind!=EDIT_SEARCH) catalog_note(pl_catalog_saved()?"Saved":"Storage full. Changes in memory.");
    show_page(destination);
}

static void editor_button(lv_event_t *e)
{
    if(field_locked) return;
    int kind=(int)(intptr_t)lv_event_get_user_data(e);
    if(kind==EDIT_TITLE && (library_current<library_first || library_current>library_last || library_current<PL_TRACK_MIN)) {
        catalog_note("Select a current track first");
        return;
    }
    editor_open(kind,0);
}

static void choose_playlist(lv_event_t *e)
{
    if(field_locked) return;
    int slot=(int)(intptr_t)lv_event_get_user_data(e);
    if(lv_event_get_code(e)==LV_EVENT_LONG_PRESSED || !*pl_catalog_playlist_name(slot)) {
        editor_open(EDIT_PLAYLIST,slot);
        return;
    }
    library_playlist=slot;
    favorites_only=false;
    library_offset=0;
    update_library();
    show_page(library);
}

static void switch_profile(lv_event_t *e)
{
    (void)e;
    if(field_locked) return;
    pl_catalog_switch_profile();
    library_playlist=-1;
    favorites_only=false;
    library_offset=0;
    library_search[0]=0;
    refresh_profiles();
    catalog_note(pl_catalog_saved()?"Pack library changed":"Storage full. Current library kept.");
    page_entered=lv_tick_get();
}

static lv_obj_t *catalog_button(lv_obj_t *page, const char *text, int x, int y, int width,
                                lv_event_cb_t action, intptr_t argument)
{
    lv_obj_t *button=menu_button(page,text,width,37,NULL);
    lv_obj_set_pos(button,x,y);
    lv_obj_add_event_cb(button,action,LV_EVENT_CLICKED,(void *)argument);
    return button;
}

static void create_catalog_pages(lv_obj_t *root)
{
    catalog_tools=subsystem_page(root,"TRACK TOOLS",library);
    playlist_page=subsystem_page(root,"PLAYLISTS",catalog_tools);
    profile_page=subsystem_page(root,"PACK PROFILES",catalog_tools);
    catalog_editor=page_new(root,MENU_BG);
    menu_panel(catalog_editor,0,0,410,51,MENU_BAR,0,false);
    navigation(catalog_tools,"PLAYLISTS",24,234,362,playlist_page);
    navigation(catalog_tools,"PACK PROFILES",24,290,362,profile_page);
    catalog_button(catalog_tools,"EDIT CURRENT TRACK TITLE",24,79,362,editor_button,EDIT_TITLE);
    catalog_button(catalog_tools,"IMPORT TITLES",24,130,362,editor_button,EDIT_IMPORT);
    catalog_button(catalog_tools,"SYNC DEVICE TITLES",24,181,362,sync_titles,0);
    catalog_notice=label_at(catalog_tools,"",24,380,0xe6b75e);
    lv_obj_set_width(catalog_notice,362);
    profile_label=label_at(profile_page,"",24,96,0x8bd89f);
    lv_obj_set_width(profile_label,362);
    catalog_button(profile_page,"NEXT PACK PROFILE",24,176,362,switch_profile,0);
    catalog_button(profile_page,"RENAME PROFILE",24,228,362,editor_button,EDIT_PROFILE);
    label_at(profile_page,"Profiles group local track metadata.",24,294,0xd2d6cd);
    label_at(playlist_page,"Tap to open. Hold to edit.",24,72,0xd2d6cd);
    for(int i=0;i<PL_PLAYLIST_LIMIT;i++) {
        playlist_buttons[i]=catalog_button(playlist_page,"",24,109+53*i,362,choose_playlist,i);
        lv_obj_remove_event_cb(playlist_buttons[i],choose_playlist);
        lv_obj_add_event_cb(playlist_buttons[i],choose_playlist,LV_EVENT_SHORT_CLICKED,(void *)(intptr_t)i);
        lv_obj_add_event_cb(playlist_buttons[i],choose_playlist,LV_EVENT_LONG_PRESSED,(void *)(intptr_t)i);
    }
    edit_prompt=label_at(catalog_editor,"",24,30,0xd2d6cd);
    edit_value=lv_textarea_create(catalog_editor);
    lv_obj_set_pos(edit_value,24,63);
    lv_obj_set_size(edit_value,362,101);
    menu_surface(edit_value,MENU_CARD,8,true);
    lv_obj_set_style_text_color(edit_value,lv_color_hex(MENU_TEXT),0);
    lv_obj_add_event_cb(edit_value,editor_focus,LV_EVENT_FOCUSED,NULL);
    lv_obj_add_event_cb(edit_value,editor_focus,LV_EVENT_CLICKED,NULL);
    edit_ids=lv_textarea_create(catalog_editor);
    lv_obj_set_pos(edit_ids,24,121);
    lv_obj_set_size(edit_ids,362,43);
    menu_surface(edit_ids,MENU_CARD,8,true);
    lv_obj_set_style_text_color(edit_ids,lv_color_hex(MENU_TEXT),0);
    lv_textarea_set_one_line(edit_ids,true);
    lv_textarea_set_max_length(edit_ids,PL_PLAYLIST_TRACKS*6);
    lv_obj_add_event_cb(edit_ids,editor_focus,LV_EVENT_FOCUSED,NULL);
    lv_obj_add_event_cb(edit_ids,editor_focus,LV_EVENT_CLICKED,NULL);
    catalog_button(catalog_editor,"BACK",24,186,174,editor_action,0);
    catalog_button(catalog_editor,"APPLY",212,186,174,editor_action,1);
    edit_keyboard=lv_keyboard_create(catalog_editor);
    lv_obj_set_size(edit_keyboard,410,260);
    menu_surface(edit_keyboard,MENU_BAR,0,false);
    lv_obj_set_style_bg_color(edit_keyboard,lv_color_hex(MENU_CARD),LV_PART_ITEMS);
    lv_obj_set_style_border_color(edit_keyboard,lv_color_hex(MENU_EDGE),LV_PART_ITEMS);
    lv_obj_set_style_text_color(edit_keyboard,lv_color_hex(MENU_TEXT),LV_PART_ITEMS);
    lv_obj_align(edit_keyboard,LV_ALIGN_BOTTOM_MID,0,0);
    lv_obj_add_event_cb(edit_keyboard,editor_action,LV_EVENT_READY,(void *)(intptr_t)1);
    lv_obj_add_event_cb(edit_keyboard,editor_action,LV_EVENT_CANCEL,NULL);
    catalog_button(library,"SEARCH",30,141,170,editor_button,EDIT_SEARCH);
    navigation(library,"TRACK TOOLS",210,138,170,catalog_tools);
    refresh_profiles();
}

void protonlink_ui_create(void)
{
    pl_catalog_load();
    load_preferences();
    lv_obj_t *root=lv_screen_active();
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    lv_color_filter_dsc_init(&disabled_filter,desaturate);
    lv_style_init(&disabled_style);
    lv_style_set_color_filter_dsc(&disabled_style,&disabled_filter);
    lv_style_set_color_filter_opa(&disabled_style,LV_OPA_COVER);
    lv_style_set_recolor_opa(&disabled_style,LV_OPA_TRANSP);
    quick=page_new(root,0x141a1b); field=page_new(root,0x020805); monitor=page_new(root,0x020805);
    menu=page_new(root,MENU_BG); library=page_new(root,MENU_BG);
    lv_obj_t *screen=quick;
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen,lv_color_hex(0x141a1b),0);
    lv_obj_set_style_text_color(screen,lv_color_hex(0xd2d6cd),0);
    lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_set_style_pad_row(screen,3,0);
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
        quick_modules[i]=panel;
        add_disabled_style(panel,LV_PART_MAIN);
        lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(panel,386,72);
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
        lv_obj_remove_flag(levels[i],LV_OBJ_FLAG_GESTURE_BUBBLE);
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
            lv_obj_set_size(leds[i][j],14,12); lv_obj_set_pos(leds[i][j],30+j*16,43);
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
        for(int part=0;part<3;part++) {
            const lv_part_t parts[]={LV_PART_MAIN,LV_PART_INDICATOR,LV_PART_KNOB};
            add_disabled_style(levels[i],parts[part]);
        }
        lv_obj_add_state(levels[i],LV_STATE_DISABLED);
        for(int tick=0;tick<=10;tick++) {
            lv_obj_t *mark=lv_obj_create(panel);
            lv_obj_remove_style_all(mark);
            lv_obj_set_size(mark,1,4);
            lv_obj_set_pos(mark,27+33*tick,65);
            lv_obj_set_style_bg_color(mark,lv_color_hex(0x78837d),0);
            lv_obj_set_style_bg_opa(mark,LV_OPA_COVER,0);
        }
    }
    lv_obj_t *transport_panel=quick_modules[3]=lv_obj_create(screen);
    lv_obj_set_size(transport_panel,386,91); lv_obj_remove_flag(transport_panel,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(transport_panel,2,0); lv_obj_set_style_pad_row(transport_panel,2,0);
    lv_obj_set_style_bg_color(transport_panel,lv_color_hex(0x141b1c),0);
    lv_obj_set_style_bg_grad_color(transport_panel,lv_color_hex(0x303838),0);
    lv_obj_set_style_bg_grad_dir(transport_panel,LV_GRAD_DIR_VER,0);
    lv_obj_set_style_border_color(transport_panel,lv_color_hex(0x65716e),0);
    lv_obj_set_style_border_width(transport_panel,1,0); lv_obj_set_style_radius(transport_panel,5,0);
    lv_obj_set_flex_flow(transport_panel,LV_FLEX_FLOW_COLUMN); lv_obj_set_flex_align(transport_panel,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_t *transport=line(transport_panel,18); lv_obj_set_width(transport,376);
    lv_obj_t *aux=lv_label_create(transport);
    lv_label_set_text(aux,"AUX TRANSPORT / CH-03");
    lv_obj_set_style_text_font(aux,&lv_font_montserrat_12,0);
    lv_obj_set_style_text_color(aux,lv_color_hex(0xa4b1a9),0);
    transport_light=lv_obj_create(transport); lv_obj_remove_style_all(transport_light); lv_obj_set_size(transport_light,10,10); lv_obj_set_style_radius(transport_light,LV_RADIUS_CIRCLE,0); lv_obj_set_style_bg_opa(transport_light,LV_OPA_COVER,0);
    lv_obj_t *track_button=make_button(transport_panel,"TRACK UNAVAILABLE",376,26,&track_label);
    lv_obj_add_event_cb(track_button,nav_event,LV_EVENT_CLICKED,library);
    lv_obj_set_size(track_label,370,22);
    lv_label_set_long_mode(track_label,LV_LABEL_LONG_DOT);
    lv_obj_set_style_bg_color(track_label,lv_color_hex(0x040a08),0);
    lv_obj_set_style_bg_opa(track_label,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(track_label,lv_color_hex(0x8bd89f),0);
    lv_obj_set_style_text_align(track_label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_t *music=line(transport_panel,35); lv_obj_set_width(music,376);
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
    action_button(pack,PL_START,"START",122,NULL);
    action_button(pack,PL_STOP,"STOP",122,NULL);
    action_button(pack,PL_VENT,"VENT",122,NULL);
    for(int i=0;i<3;i++) {
        lv_obj_set_height(buttons[i],62);
        lv_obj_t *indicator=lv_obj_create(buttons[i]); lv_obj_remove_style_all(indicator);
        lv_obj_set_size(indicator,96,10); lv_obj_align(indicator,LV_ALIGN_TOP_MID,0,0);
        lv_obj_set_style_bg_color(indicator,lv_color_hex(0x0b1011),0); lv_obj_set_style_bg_opa(indicator,LV_OPA_COVER,0);
        lv_obj_set_style_border_color(indicator,lv_color_hex(switch_color[i]),0); lv_obj_set_style_border_width(indicator,1,0); lv_obj_set_style_radius(indicator,2,0);
        lv_obj_remove_flag(indicator,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *lamp=lv_obj_create(indicator); lv_obj_remove_style_all(lamp); lv_obj_set_size(lamp,6,6); lv_obj_center(lamp);
        lv_obj_set_style_radius(lamp,LV_RADIUS_CIRCLE,0); lv_obj_set_style_bg_color(lamp,lv_color_hex(switch_color[i]),0); lv_obj_set_style_bg_opa(lamp,LV_OPA_COVER,0); lv_obj_remove_flag(lamp,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(lv_obj_get_child(buttons[i],0),LV_ALIGN_CENTER,0,6);
        pack_progress[i]=progress_at(buttons[i],100,-2,switch_color[i]);
    }
    status_label=lv_label_create(screen); lv_obj_set_style_text_color(status_label,lv_color_hex(0xe6b75e),0);
    lv_obj_set_style_text_font(status_label,&lv_font_montserrat_12,0);
    lv_obj_t *foot=lv_label_create(screen); lv_label_set_text(foot,"FIELD CONTROL / HOLD LIGHTS TO LOCK");
    lv_obj_set_style_text_font(foot,&lv_font_montserrat_12,0); lv_obj_set_style_text_color(foot,lv_color_hex(0xe6b75e),0);
    field_face=pl_face_create(field,56,386);
    monitor_face=pl_face_create(monitor,56,386);
    label_at(field,"LINK / PL-01",16,15,0x82b693);
    lock_label=label_at(field,"HOLD TO UNLOCK",0,0,MENU_AMBER);
    lv_obj_set_width(lock_label,370);
    lv_obj_set_style_text_align(lock_label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_letter_space(lock_label,1,0);
    lv_obj_align(lock_label,LV_ALIGN_BOTTOM_MID,0,-22);
    field_progress=progress_at(field,370,-8,0x85e7aa);
    lv_obj_add_flag(field,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(field,gesture_event,LV_EVENT_ALL,(void *)(intptr_t)G_FIELD);
    label_at(monitor,"PROTONLINK",20,17,0xd2d6cd);
    monitor_dots=make_dots(monitor); lv_obj_set_pos(monitor_dots,280,10);
    monitor_progress=progress_at(monitor_dots,100,0,0xe6b75e);
    lv_obj_add_event_cb(monitor_dots,gesture_event,LV_EVENT_ALL,(void *)(intptr_t)G_MONITOR);
    label_at(monitor,"PKE MONITOR / TAP LIGHTS TO RETURN",35,465,0xe6b75e);
    create_subsystems(root);
    menu_title(library,"AUX TRACK DATABASE",41);
    library_return=menu_button(library,"RETURN TO FIELD CONTROL",350,39,NULL);
    lv_obj_set_pos(library_return,30,49);
    lv_obj_add_event_cb(library_return,library_event,LV_EVENT_CLICKED,(void *)(intptr_t)RETURN_FROM_LIBRARY);
    library_track=label_at(library,"TRACK UNAVAILABLE",30,184,0x8bd89f);
    lv_obj_set_width(library_track,350); lv_label_set_long_mode(library_track,LV_LABEL_LONG_DOT);
    library_filter=menu_button(library,"ALL TRACKS",170,34,NULL);
    lv_obj_set_pos(library_filter,30,96);
    lv_obj_add_event_cb(library_filter,library_event,LV_EVENT_CLICKED,(void *)(intptr_t)FILTER_TRACKS);
    library_favorite=menu_button(library,"STAR CURRENT",170,34,NULL);
    lv_obj_set_pos(library_favorite,210,96);
    lv_obj_add_event_cb(library_favorite,library_event,LV_EVENT_CLICKED,(void *)(intptr_t)STAR_CURRENT);
    for(int i=0;i<7;i++) {
        library_rows[i]=menu_button(library,"",350,32,&library_names[i]); lv_obj_set_pos(library_rows[i],30,208+i*34);
        lv_obj_set_style_bg_color(library_rows[i],lv_color_hex(0x27443c),LV_STATE_USER_1);
        lv_obj_set_style_border_color(library_rows[i],lv_color_hex(0x537c68),LV_STATE_USER_1);
        lv_obj_set_width(library_names[i],318); lv_label_set_long_mode(library_names[i],LV_LABEL_LONG_DOT);
        lv_obj_add_event_cb(library_rows[i],library_event,LV_EVENT_CLICKED,(void *)(intptr_t)i);
    }
    library_empty=label_at(library,"NO MATCHING TRACKS",0,246,MENU_MUTED);
    lv_obj_set_width(library_empty,410);
    lv_obj_set_style_text_align(library_empty,LV_TEXT_ALIGN_CENTER,0);
    library_prev=menu_button(library,"PREVIOUS PAGE",170,34,NULL); lv_obj_set_pos(library_prev,30,457);
    library_next=menu_button(library,"NEXT PAGE",170,34,NULL); lv_obj_set_pos(library_next,210,457);
    lv_obj_add_event_cb(library_prev,library_event,LV_EVENT_CLICKED,(void *)(intptr_t)PREVIOUS_PAGE);
    lv_obj_add_event_cb(library_next,library_event,LV_EVENT_CLICKED,(void *)(intptr_t)NEXT_PAGE);
    create_catalog_pages(root);
    lv_obj_t *swipe_pages[]={quick,monitor,music_page,diagnostics};
    for(int i=0;i<4;i++) {
        lv_obj_remove_flag(swipe_pages[i],LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_flag(swipe_pages[i],LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(swipe_pages[i],subsystem_swipe,LV_EVENT_GESTURE,NULL);
    }
    engage_lock();
    lv_timer_create(gesture_tick,20,NULL);
    refresh(NULL);
    lv_timer_create(refresh,250,NULL);
}
