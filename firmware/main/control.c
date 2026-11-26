#include "control.h"
#include <string.h>
const char *pl_command_path(pl_action_t a, const protonlink_status_t *s)
{
    if (!s || !s->online) return NULL;
    bool normal = !s->firing && !s->lockout && !s->ramping &&
        !strcmp(s->temperature, "Normal") &&
        (!strcmp(s->cyclotron, "Normal") || !strcmp(s->cyclotron, "Active"));
    bool known_transition = s->transition_known || s->legacy_superhero;
    switch (a) {
    case PL_START: return normal && known_transition && !s->pack_on ? "/pack/on" : NULL;
    case PL_STOP: return normal && known_transition && s->pack_on ? "/pack/off" : NULL;
    case PL_VENT: return normal && known_transition && s->pack_on ? "/pack/vent" : NULL;
    case PL_MASTER_UP: return s->master < 100 ? "/volume/master/up" : NULL;
    case PL_MASTER_DOWN: return s->master > 0 ? "/volume/master/down" : NULL;
    case PL_EFFECTS_UP: return s->effects < 100 ? "/volume/effects/up" : NULL;
    case PL_EFFECTS_DOWN: return s->effects > 0 ? "/volume/effects/down" : NULL;
    case PL_MUSIC_UP: return s->music < 100 ? "/volume/music/up" : NULL;
    case PL_MUSIC_DOWN: return s->music > 0 ? "/volume/music/down" : NULL;
    case PL_PLAY_STOP: return s->music_known ? "/music/startstop" : NULL;
    case PL_PAUSE_RESUME: return s->music_known && s->playing ? "/music/pauseresume" : NULL;
    case PL_PREVIOUS: return s->music_known && s->track >= 500 ? "/music/prev" : NULL;
    case PL_NEXT: return s->music_known && s->track >= 500 ? "/music/next" : NULL;
    default: return NULL;
    }
}
