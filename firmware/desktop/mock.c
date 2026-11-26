#include "control.h"
#include "mock.h"
#include "lvgl.h"
#include <string.h>
static bool unlocked;
static uint32_t vent_start;
static bool venting;
static protonlink_status_t state={.online=true,.music_known=true,.transition_known=true,.legacy_superhero=true,.track=500,.master=30,.effects=100,.music=75,.temperature="Normal",.cyclotron="Normal"};
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
