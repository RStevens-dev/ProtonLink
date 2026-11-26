#include "control.h"
#include <assert.h>
#include <string.h>
int main(void) {
    protonlink_status_t s = {.online=true,.transition_known=true,.master=50,.effects=50,.music=50,.music_known=true,.track=500};
    strcpy(s.temperature,"Normal"); strcpy(s.cyclotron,"Normal");
    assert(!strcmp(pl_command_path(PL_START,&s),"/pack/on"));
    assert(!pl_command_path(PL_STOP,&s));
    assert(!pl_command_path(PL_VENT,&s));
    s.pack_on=true;
    assert(!strcmp(pl_command_path(PL_VENT,&s),"/pack/vent"));
    s.firing=true;
    assert(!pl_command_path(PL_VENT,&s));
    assert(!pl_command_path(PL_STOP,&s));
    s.firing=false; s.ramping=true;
    assert(!pl_command_path(PL_VENT,&s));
    s.ramping=false; s.transition_known=false;
    assert(!pl_command_path(PL_VENT,&s));
    s.legacy_superhero=true;
    assert(pl_command_path(PL_VENT,&s));
    s.lockout=true;
    assert(!pl_command_path(PL_VENT,&s));
    s.lockout=false; strcpy(s.temperature,"Unknown");
    assert(!pl_command_path(PL_VENT,&s));
    s.master=100; assert(!pl_command_path(PL_MASTER_UP,&s));
    s.effects=0; assert(!pl_command_path(PL_EFFECTS_DOWN,&s));
    assert(pl_command_path(PL_PLAY_STOP,&s));
    assert(!pl_command_path(PL_PAUSE_RESUME,&s));
    s.playing=true; assert(pl_command_path(PL_PAUSE_RESUME,&s));
    s.online=false; assert(!pl_command_path(PL_NEXT,&s));
    s.online=true; s.master=50; s.effects=50; s.music=50;
    assert(!strcmp(pl_volume_step(&s,0,75),"/volume/master/up"));
    assert(!strcmp(pl_volume_step(&s,1,25),"/volume/effects/down"));
    assert(!strcmp(pl_volume_step(&s,2,75),"/volume/music/up"));
    assert(!pl_volume_step(&s,2,50));
    assert(!pl_volume_step(&s,-1,50));
    assert(!pl_volume_step(&s,3,50));
    assert(!pl_volume_step(&s,0,101));
    s.online=false; assert(!pl_volume_step(&s,0,75));
    s.online=true; s.track_first=500; s.track_last=550;
    assert(pl_track_allowed(&s,500)); assert(pl_track_allowed(&s,550));
    assert(!pl_track_allowed(&s,499)); assert(!pl_track_allowed(&s,551));
    s.online=false; assert(!pl_track_allowed(&s,500));
    return 0;
}
