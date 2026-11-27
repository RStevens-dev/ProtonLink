#include "options.h"
#include <string.h>

typedef struct { const char *path, *alternate, *label, *alternate_label, *value; } option_t;
static const option_t options[PL_OPT_COUNT]={
    [PL_OPT_MUTE]={"/volume/mute","/volume/unmute","MUTE","UNMUTE"},
    [PL_OPT_REPEAT]={"/music/loop/one","/music/loop/all","REPEAT ALL","REPEAT ONE"},
    [PL_OPT_SHUFFLE]={"/music/shuffle/on","/music/shuffle/off","SHUFFLE OFF","SHUFFLE ON"},
    [PL_OPT_SMOKE]={"/pack/smoke/on","/pack/smoke/off","SMOKE OFF","SMOKE ON"},
    [PL_OPT_VIBRATION]={"/pack/vibration/on","/pack/vibration/off","VIBRATION OFF","VIBRATION ON"},
    [PL_OPT_DIRECTION]={"/pack/cyclotron/clockwise","/pack/cyclotron/counterclockwise","COUNTERCLOCKWISE","CLOCKWISE"},
    [PL_OPT_1984]={"/pack/theme/1984",NULL,"1984 / ORIGINAL",NULL,"1984"},
    [PL_OPT_1989]={"/pack/theme/1989",NULL,"1989 / GHOSTBUSTERS II",NULL,"1989"},
    [PL_OPT_2021]={"/pack/theme/2021",NULL,"2021 / AFTERLIFE",NULL,"2021"},
    [PL_OPT_2024]={"/pack/theme/2024",NULL,"2024 / FROZEN EMPIRE",NULL,"2024"},
    [PL_OPT_PROTON]={"/pack/stream/proton",NULL,"PROTON STREAM",NULL,"Proton Stream"},
    [PL_OPT_STASIS]={"/pack/stream/stasis",NULL,"STASIS STREAM",NULL,"Stasis Stream"},
    [PL_OPT_SLIME]={"/pack/stream/slime",NULL,"SLIME TETHER",NULL,"Slime Tether"},
    [PL_OPT_MESON]={"/pack/stream/meson",NULL,"MESON COLLIDER",NULL,"Meson Collider"},
    [PL_OPT_SPECTRAL]={"/pack/stream/spectral",NULL,"SPECTRAL STREAM",NULL,"Spectral Stream"},
    [PL_OPT_HALLOWEEN]={"/pack/stream/holiday/halloween",NULL,"HALLOWEEN",NULL,"Halloween"},
    [PL_OPT_CHRISTMAS]={"/pack/stream/holiday/christmas",NULL,"CHRISTMAS",NULL,"Christmas"},
    [PL_OPT_CUSTOM]={"/pack/stream/spectral/custom",NULL,"CUSTOM SPECTRAL",NULL,"Custom Spectral"}
};

const char *pl_option_candidate(pl_option_t o,bool second)
{
    if(o<0 || o>=PL_OPT_COUNT) return NULL;
    return second?options[o].alternate:options[o].path;
}

const char *pl_option_label(pl_option_t o,const protonlink_status_t *s)
{
    if(o<0 || o>=PL_OPT_COUNT) return "";
    const option_t *entry=&options[o];
    return entry->alternate_label && (s->option_values & (1u<<o))?entry->alternate_label:entry->label;
}

const char *pl_option_path(pl_option_t o,const protonlink_status_t *s)
{
    if(!s || !s->online || o<0 || o>=PL_OPT_COUNT || !(s->advertised_options & (1u<<o))) return NULL;
    const option_t *entry=&options[o];
    if(o<=PL_OPT_DIRECTION) {
        if(!(s->known_options & (1u<<o))) return NULL;
        if(o>=PL_OPT_SMOKE && (!s->transition_known || s->ramping || s->firing || s->lockout ||
                              strcmp(s->temperature,"Normal"))) return NULL;
        return (s->option_values & (1u<<o))?entry->alternate:entry->path;
    }
    if(!s->transition_known || s->ramping || s->firing || s->lockout || strcmp(s->temperature,"Normal")) return NULL;
    if(o<=PL_OPT_2024) {
        if(!s->modern_pack || s->pack_on || strcmp(s->wand_power,"Idle") || !strcmp(s->theme,"Unknown") ||
           !strcmp(s->theme,entry->value)) return NULL;
    } else {
        if(!s->pack_on || !s->stream_change_reported || !s->vg_mode || strcmp(s->cable,"Connected") ||
           (strcmp(s->mode,"Super Hero") && strcmp(s->mode,"Upgraded")) ||
           (strcmp(s->cyclotron,"Normal") && strcmp(s->cyclotron,"Active")) ||
           !strcmp(s->stream,entry->value)) return NULL;
    }
    return entry->path;
}
