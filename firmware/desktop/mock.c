#include "control.h"
#include "options.h"
#include "mock.h"
#include "lvgl.h"
#include <string.h>
static bool unlocked;
static uint32_t vent_start;
static bool venting;
static protonlink_status_t state={.online=true,.music_known=true,.transition_known=true,.legacy_superhero=true,.power_level=3,.known_options=0x3f,.advertised_options=(1u<<PL_OPT_COUNT)-1,.modern_pack=true,.vg_mode=true,.stream_change_reported=true,.track=500,.track_first=500,.track_last=550,.master=30,.effects=100,.music=75,.lid_known=true,.lid_fitted=true,.mode="Super Hero",.theme="1984",.stream="Proton Stream",.wand="Connected",.wand_power="Idle",.cable="Connected",.barrel="Retracted",.temperature="Normal",.cyclotron="Normal"};
void pl_set_unlocked(bool value){unlocked=value && state.online;}
bool pl_is_unlocked(void){return unlocked;}
bool pl_busy(void){return false;}
bool protonlink_snapshot(protonlink_status_t *out){
    if(venting && lv_tick_elaps(vent_start)>=4000){venting=false;strcpy(state.temperature,"Normal");}
    if(!out || !state.online)return false;
    *out=state;return true;
}
bool pl_request_volume(int channel,int target){
    if(!unlocked || !pl_volume_step(&state,channel,target))return false;
    int rounded=(target+2)/5*5;
    switch(channel){case 0:state.master=rounded;break;case 1:state.effects=rounded;break;case 2:state.music=rounded;break;default:return false;}
    return true;
}
bool pl_request(pl_action_t a){
    if(!unlocked || !pl_command_path(a,&state))return false;
    switch(a){
        case PL_START:state.pack_on=true;break;
        case PL_STOP:state.pack_on=false;break;
        case PL_VENT:vent_start=lv_tick_get();venting=true;strcpy(state.temperature,"Venting");break;
        case PL_PLAY_STOP:state.playing=!state.playing;state.paused=false;break;
        case PL_PAUSE_RESUME:state.paused=!state.paused;break;
        case PL_PREVIOUS:if(state.track>500)state.track--;break;
        case PL_NEXT:if(state.track<550)state.track++;break;
        default:return false;
    }return true;
}
void mock_scenario(int scenario){
    state.online=scenario!=0;state.firing=scenario==2;state.lockout=scenario==3;
    state.pack_on=scenario==2 || scenario==3 || state.pack_on;
    if(!state.online)unlocked=false;
    strcpy(state.cyclotron,scenario==3?"Critical":"Normal");
}

bool pl_request_track(int track) {
    if(!unlocked || !pl_track_allowed(&state,track)) return false;
    state.track=track; return true;
}

static bool sync_requested;
bool protonlink_request_song_list(void)
{
    sync_requested=state.online;
    return sync_requested;
}
pl_sync_state_t protonlink_song_list(char *out, size_t capacity, int *first, int *last)
{
    if(!sync_requested) return PL_SYNC_IDLE;
    if(!state.online) return PL_SYNC_UNAVAILABLE;
    if(!out) return PL_SYNC_READY;
    const char titles[]="500 Proton March\n501 Firehouse Theme\n502 Ecto Patrol";
    if(capacity<sizeof(titles)) return PL_SYNC_UNAVAILABLE;
    memcpy(out,titles,sizeof(titles));
    *first=state.track_first;
    *last=state.track_last;
    sync_requested=false;
    return PL_SYNC_READY;
}

bool pl_request_option(pl_option_t option)
{
    if(!unlocked || !pl_option_path(option,&state)) return false;
    const char *const themes[]={"1984","1989","2021","2024"};
    const char *const streams[]={"Proton Stream","Stasis Stream","Slime Tether","Meson Collider","Spectral Stream",
                                 "Halloween","Christmas","Custom Spectral"};
    if(option<=PL_OPT_DIRECTION) state.option_values^=1u<<option;
    else if(option<=PL_OPT_2024) strcpy(state.theme,themes[option-PL_OPT_1984]);
    else strcpy(state.stream,streams[option-PL_OPT_PROTON]);
    return true;
}
